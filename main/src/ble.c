/**
 * @file ble.c
 * @brief BLE 透传实现 (NimBLE GATT Server)
 *
 * 设计:
 *   - 使用 HM-10 兼容的 16-bit 服务/特征: 服务 0xFFE0, 写特征 0xFFE1,
 *     便于用常见串口透传类 App 直接测试
 *   - 手机写入的字节在写特征回调中整包取出, 再转发给应用层回调
 *   - 广播采用可连接 + 通用可发现, 断连后自动恢复广播
 *   - NimBLE 主机任务固定在 core 0, 避免干扰 core 1 上的传感器时序
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "ble.h"

#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "store/config/ble_store_config.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

#define BLE_TAG "ble"

/* NimBLE 无头文件声明此函数, 需手动声明 */
void ble_store_config_init(void);

/** GATT 服务/特征 UUID (HM-10 兼容) */
static const ble_uuid16_t wm_svc_uuid = BLE_UUID16_INIT(0xFFE0);
static const ble_uuid16_t wm_rx_uuid  = BLE_UUID16_INIT(0xFFE1);

static uint16_t wm_rx_val_handle;

static wm_ble_data_cb_t s_data_cb = NULL;
static uint8_t own_addr_type;

static volatile bool s_ble_enabled = false; /**< 蓝牙开关状态 */
static volatile bool s_connected    = false; /**< 是否已建立连接 */
static volatile uint16_t s_conn_handle;     /**< 当前连接句柄 */

/** @brief 蓝牙控制命令(经队列发给专用控制任务, 避免占用调用方栈) */
typedef enum {
    WM_BLE_CMD_START,
    WM_BLE_CMD_STOP,
} wm_ble_cmd_t;

#define WM_BLE_CMD_QUEUE_LEN 4
#define WM_BLE_CTRL_TASK_STACK 4096

static QueueHandle_t s_ble_cmd_queue = NULL;

static void start_advertising(void);
static int wm_gap_event_handler(struct ble_gap_event *event, void *arg);
static void wm_ble_ctrl_task(void *arg);

static int wm_rx_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint8_t buf[WM_BLE_RX_BUF_SIZE];
    uint16_t len = 0;
    int rc;

    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    /* 官方写法: 把整条 mbuf 链拷贝为连续字节 */
    rc = ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf), &len);
    if (rc != 0 && rc != BLE_HS_EMSGSIZE) {
        ESP_LOGE(BLE_TAG, "failed to copy rx data, rc=%d", rc);
        return BLE_ATT_ERR_UNLIKELY;
    }

    ESP_LOGI(BLE_TAG, "rx %d bytes (conn_handle=%d): %.*s", len, conn_handle,
             (int)len, (const char *)buf);

    if (s_data_cb) {
        s_data_cb(buf, len);
    }

    return 0;
}

static const struct ble_gatt_svc_def wm_gatt_svcs[] = {
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = &wm_svc_uuid.u,
     .characteristics =
         (struct ble_gatt_chr_def[]){
             {.uuid = &wm_rx_uuid.u,
              .access_cb = wm_rx_access_cb,
              .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
              .val_handle = &wm_rx_val_handle},
             {0},
         }},
    {0},
};

static void start_advertising(void)
{
    struct ble_hs_adv_fields adv_fields = {0};
    struct ble_gap_adv_params adv_params = {0};
    const char *name;
    int rc;

    adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    name = ble_svc_gap_device_name();
    adv_fields.name = (uint8_t *)name;
    adv_fields.name_len = strlen(name);
    adv_fields.name_is_complete = 1;

    rc = ble_gap_adv_set_fields(&adv_fields);
    if (rc != 0) {
        ESP_LOGE(BLE_TAG, "failed to set advertising data, error code: %d", rc);
        return;
    }

    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(200);
    adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(250);

    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params,
                           wm_gap_event_handler, NULL);
    if (rc != 0) {
        ESP_LOGE(BLE_TAG, "failed to start advertising, error code: %d", rc);
        return;
    }
    ESP_LOGI(BLE_TAG, "advertising started");
}

static int wm_gap_event_handler(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            s_connected = true;
            s_conn_handle = event->connect.conn_handle;
            ESP_LOGI(BLE_TAG, "connected");
        } else {
            if (s_ble_enabled) {
                start_advertising();
            }
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        s_connected = false;
        ESP_LOGI(BLE_TAG, "disconnected; reason=%d", event->disconnect.reason);
        if (s_ble_enabled) {
            start_advertising();
        }
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        if (s_ble_enabled) {
            start_advertising();
        }
        return 0;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(BLE_TAG, "mtu updated; conn_handle=%d mtu=%d",
                 event->mtu.conn_handle, event->mtu.value);
        return 0;

    default:
        return 0;
    }
}

static void wm_on_stack_sync(void)
{
    int rc;

    rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(BLE_TAG, "device does not have any available bt address!");
        return;
    }

    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(BLE_TAG, "failed to infer address type, error code: %d", rc);
        return;
    }

    if (s_ble_enabled) {
        start_advertising();
    }
}

static void wm_on_stack_reset(int reason)
{
    ESP_LOGI(BLE_TAG, "nimble stack reset, reason: %d", reason);
}

static void wm_gatt_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    switch (ctxt->op) {
    case BLE_GATT_REGISTER_OP_SVC:
        ESP_LOGI(BLE_TAG, "service registered");
        break;
    case BLE_GATT_REGISTER_OP_CHR:
        ESP_LOGI(BLE_TAG, "characteristic registered");
        break;
    default:
        break;
    }
}

static void wm_host_config_init(void)
{
    ble_hs_cfg.reset_cb = wm_on_stack_reset;
    ble_hs_cfg.sync_cb = wm_on_stack_sync;
    ble_hs_cfg.gatts_register_cb = wm_gatt_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    ble_store_config_init();
}

static void wm_host_task(void *param)
{
    nimble_port_run();
    vTaskDelete(NULL);
}

esp_err_t wm_ble_init(void)
{
    esp_err_t ret;
    int rc;

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(BLE_TAG, "failed to initialize nvs flash, error code: %d",
                 ret);
        return ret;
    }

    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(BLE_TAG, "failed to initialize nimble stack, error code: %d",
                 ret);
        return ret;
    }

    ble_svc_gap_init();
    ble_svc_gatt_init();

    rc = ble_svc_gap_device_name_set(WM_BLE_DEVICE_NAME);
    if (rc != 0) {
        ESP_LOGE(BLE_TAG, "failed to set device name to %s, error code: %d",
                 WM_BLE_DEVICE_NAME, rc);
        return ESP_FAIL;
    }

    rc = ble_gatts_count_cfg(wm_gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(BLE_TAG, "failed to count gatt services, error code: %d", rc);
        return ESP_FAIL;
    }

    rc = ble_gatts_add_svcs(wm_gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(BLE_TAG, "failed to add gatt services, error code: %d", rc);
        return ESP_FAIL;
    }

    wm_host_config_init();

    if (xTaskCreatePinnedToCore(wm_host_task, "nimble_host", 4096, NULL, 5,
                                NULL, 0) != pdPASS) {
        ESP_LOGE(BLE_TAG, "failed to create nimble host task");
        return ESP_FAIL;
    }

    s_ble_cmd_queue = xQueueCreate(WM_BLE_CMD_QUEUE_LEN, sizeof(wm_ble_cmd_t));
    if (!s_ble_cmd_queue) {
        ESP_LOGE(BLE_TAG, "failed to create ble ctrl queue");
        return ESP_FAIL;
    }

    if (xTaskCreatePinnedToCore(wm_ble_ctrl_task, "ble_ctrl", WM_BLE_CTRL_TASK_STACK,
                                NULL, 5, NULL, 0) != pdPASS) {
        ESP_LOGE(BLE_TAG, "failed to create ble ctrl task");
        return ESP_FAIL;
    }

    ESP_LOGI(BLE_TAG, "ble initialized, device name: %s", WM_BLE_DEVICE_NAME);
    return ESP_OK;
}

/** @brief 蓝牙控制任务: 在独立任务里执行 NimBLE 启停, 栈充足 */
static void wm_ble_ctrl_task(void *arg)
{
    wm_ble_cmd_t cmd;

    while (1) {
        if (xQueueReceive(s_ble_cmd_queue, &cmd, portMAX_DELAY) != pdPASS) {
            continue;
        }

        if (cmd == WM_BLE_CMD_START) {
            s_ble_enabled = true;
            if (!s_connected) {
                start_advertising();
            }
        } else {
            s_ble_enabled = false;
            ble_gap_adv_stop();
            if (s_connected) {
                ble_gap_terminate(s_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
            }
        }
    }
}

void wm_ble_start(void)
{
    wm_ble_cmd_t cmd = WM_BLE_CMD_START;

    if (s_ble_cmd_queue) {
        xQueueSend(s_ble_cmd_queue, &cmd, 0);
    }
}

void wm_ble_stop(void)
{
    wm_ble_cmd_t cmd = WM_BLE_CMD_STOP;

    if (s_ble_cmd_queue) {
        xQueueSend(s_ble_cmd_queue, &cmd, 0);
    }
}

void wm_ble_set_data_cb(wm_ble_data_cb_t cb)
{
    s_data_cb = cb;
}
