/**
 * @file wifi_sta.c
 * @brief WiFi STA 驱动实现 (esp_wifi + esp_netif)
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "wifi_sta.h"

#define WIFI_TAG "wifi_sta"

#define WIFI_NVS_NAMESPACE  "wifi_cfg"
#define WIFI_NVS_KEY_SSID   "ssid"
#define WIFI_NVS_KEY_PASS   "pass"

#define WIFI_CONNECT_RETRY      5
#define WIFI_RETRY_DELAY_MS     2000

static char s_ssid[WIFI_MAX_SSID_LEN + 1] = {0};
static char s_pass[WIFI_MAX_PASS_LEN + 1] = {0};

static volatile bool s_started    = false; /**< WiFi 已启动 */
static volatile bool s_connected  = false; /**< 已连接并获取 IP */
static int s_retry_count          = 0;

static void wifi_set_config(const char *ssid, const char *password)
{
    wifi_config_t cfg = {0};

    memcpy(cfg.sta.ssid, ssid, strlen(ssid));
    memcpy(cfg.sta.password, password, strlen(password));
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
}

static esp_err_t wifi_save_creds(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_str(handle, WIFI_NVS_KEY_SSID, s_ssid);
    if (err == ESP_OK) {
        err = nvs_set_str(handle, WIFI_NVS_KEY_PASS, s_pass);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

static esp_err_t wifi_load_creds(void)
{
    nvs_handle_t handle;
    size_t len;
    esp_err_t err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }

    len = sizeof(s_ssid);
    err = nvs_get_str(handle, WIFI_NVS_KEY_SSID, s_ssid, &len);
    if (err == ESP_OK) {
        len = sizeof(s_pass);
        err = nvs_get_str(handle, WIFI_NVS_KEY_PASS, s_pass, &len);
    }
    nvs_close(handle);
    return err;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        s_started = true;
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *d = event_data;

        ESP_LOGW(WIFI_TAG, "disconnected, reason=%d", d->reason);
        s_connected = false;

        if (s_retry_count < WIFI_CONNECT_RETRY && s_ssid[0] != '\0') {
            s_retry_count++;
            ESP_LOGI(WIFI_TAG, "retry %d/%d to connect", s_retry_count,
                     WIFI_CONNECT_RETRY);
            vTaskDelay(pdMS_TO_TICKS(WIFI_RETRY_DELAY_MS));
            esp_wifi_connect();
        } else {
            ESP_LOGE(WIFI_TAG, "connect to %s failed after %d retries",
                     s_ssid, WIFI_CONNECT_RETRY);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = event_data;

        ESP_LOGI(WIFI_TAG, "connected to %s, got ip: " IPSTR,
                 s_ssid, IP2STR(&e->ip_info.ip));
        s_connected = true;
        s_retry_count = 0;
    }
}

esp_err_t wifi_sta_init(void)
{
    esp_err_t err;

    /* 复用/确保 NVS 已初始化 */
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "nvs init failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    if (wifi_load_creds() == ESP_OK && s_ssid[0] != '\0') {
        wifi_set_config(s_ssid, s_pass);
        ESP_LOGI(WIFI_TAG, "saved creds found, auto connecting to %s", s_ssid);
        ESP_ERROR_CHECK(esp_wifi_start());
    }

    return ESP_OK;
}

esp_err_t wifi_sta_connect(const char *ssid, const char *password)
{
    esp_err_t err;

    if (!ssid || ssid[0] == '\0' || strlen(ssid) > WIFI_MAX_SSID_LEN) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!password || strlen(password) > WIFI_MAX_PASS_LEN) {
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(s_ssid, ssid, WIFI_MAX_SSID_LEN);
    strncpy(s_pass, password, WIFI_MAX_PASS_LEN);
    s_ssid[WIFI_MAX_SSID_LEN] = '\0';
    s_pass[WIFI_MAX_PASS_LEN] = '\0';
    s_retry_count = 0;

    wifi_set_config(s_ssid, s_pass);

    err = wifi_save_creds();
    if (err != ESP_OK) {
        ESP_LOGW(WIFI_TAG, "failed to save creds to nvs: %s",
                 esp_err_to_name(err));
    }

    ESP_LOGI(WIFI_TAG, "connecting to %s", s_ssid);

    if (!s_started) {
        return esp_wifi_start();
    }
    return esp_wifi_connect();
}

bool wifi_sta_is_connected(void)
{
    return s_connected;
}

void wifi_sta_clear(void)
{
    /* 擦除 NVS 中保存的 WiFi 凭据 */
    nvs_handle_t handle;
    if (nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_erase_all(handle);
        nvs_commit(handle);
        nvs_close(handle);
    }

    s_ssid[0] = '\0';
    s_pass[0] = '\0';
    s_connected = false;
    s_retry_count = 0;

    if (s_started) {
        esp_wifi_stop();
        s_started = false;
    }
}
