/**
 * @file main.c
 * @brief 多传感器监测: 温度 + 水流量 + EC 电导率 + 浊度, 数码管显示, LED 指示
 *
 * 硬件连接:
 *   - DS18B20 数据线: GPIO5 (需 4.7k 上拉到 3.3V)
 *   - YF-S201 脉冲线: GPIO6 (集电极开路输出, 需上拉)
 *   - EC 模拟输出:    GPIO4 (ADC1_CH3)
 *   - 浊度 AO:        GPIO7 (ADC1_CH6); GPIO15 不用, DO 不接
 *   - 数码管: 见 seg.h (位选 IO11/12, 段选 IO18/16/9/3/8/17/10/46)
 *   - LED:   见 led.h (A0/A1/A2 -> IO35/36/37)
 *   - 按键:  SW1 -> IO48 (key.h KEY_0), SW2 -> IO45 (key.h KEY_1, 蓝牙开关)
 *   - 蜂鸣器: 见 buzzer.h (IO2, 高电平鸣响)
 *
 * 软件结构: 双核分工
 *   - 传感器核 (core 1): 专用任务周期性调用各传感器读取函数
 *       (ds18b20 转换/读取, ec 读取, 浊度读取, yf_s201 流量读取),
 *       保证传感器时序不被其他任务干扰; 结果发布到各自共享变量。
 *   - 控制核 (core 0): 专用任务负责数码管扫描、LED 刷新、按键扫描、蜂鸣器控制,
 *       以 2ms 周期运行, 根据共享变量和按键状态切换显示。
 *   两任务固定到不同核, 互不抢占: 1-Wire 位时序与脉冲计数不受显示扫描影响。
 *
 * 显示切换:
 *   - 开机默认显示温度, 按 SW1 循环切换: 温度 -> 水流量 -> EC -> 浊度 -> 温度...
 *   - LED 指示: 温度 0x01(LED0), 水流量 0x02(LED1), EC 0x04(LED2), 浊度 0x08(LED3)
 *
 * 蓝牙控制:
 *   - 开机默认关闭蓝牙(不广播), 按 SW2(KEY_1) 开/关蓝牙
 *   - 蓝牙开启时 LED7(0x80) 点亮, 关闭后熄灭
 *
 * 显示格式: 温度/流量 XX.X, EC 整数(μS/cm), 浊度整数(0~100); 失败显示 "EEEE"
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "seg.h"
#include "ds18b20.h"
#include "yf_s201.h"
#include "ec.h"
#include "turbidity.h"
#include "led.h"
#include "key.h"
#include "ble.h"
#include "wifi_sta.h"
#include "http_upload.h"
#include "buzzer.h"
#include "alarm_config.h"
#include "device_cfg.h"

#define TAG "sensor_disp"

#define CONVERSION_DELAY_MS 750 /**< DS18B20 12bit 转换时间 */
#define HOLD_DELAY_MS       250 /**< 传感器两次采样间间隔 */

#define ERR_DISP (-32768) /**< 共享值中表示读取失败的标志: 取不可能出现的值, 避免与合法负温度(-1)冲突 */

#define LED_TEMP 0x01 /**< 指示位: 数码管显示温度 */
#define LED_FLOW 0x02 /**< 指示位: 数码管显示水流量 */
#define LED_EC   0x04 /**< 指示位: 数码管显示电导率 */
#define LED_TURB 0x08 /**< 指示位: 数码管显示浊度 */
#define LED_BLE  0x80 /**< 指示位: 蓝牙开启指示(LED7, 最高位) */
#define LED_WIFI 0x40 /**< 指示位: WiFi 已连接指示(LED6) */

/** @brief 显示通道索引 */
enum {
    SEL_TEMP = 0,
    SEL_FLOW,
    SEL_EC,
    SEL_TURB,
    SEL_NUM
};

/** @brief 各显示通道对应的 LED 指示位 */
static const uint8_t SEL_LED[SEL_NUM] = {
    [SEL_TEMP] = LED_TEMP,
    [SEL_FLOW] = LED_FLOW,
    [SEL_EC]   = LED_EC,
    [SEL_TURB] = LED_TURB,
};

/** @brief 各显示通道的小数点位置: 2 = XX.X, -1 = 整数显示 */
static const int SEL_POINT[SEL_NUM] = {
    [SEL_TEMP] = 2,
    [SEL_FLOW] = 2,
    [SEL_EC]   = -1,
    [SEL_TURB] = -1,
};

/** @brief 共享显示值: -1 表示错误; 传感器任务写, 控制任务读
 *         temp/flow 为数值*10, ec 为整数 μS/cm, turb 为整数(0~100) */
static volatile int g_temp = 0;
static volatile int g_flow = 0;
static volatile int g_ec   = 0;
static volatile int g_turb = 0;

/** @brief 报警去抖计数: 连续多少次采样满足报警条件才真正鸣响(防开机/瞬时误报) */
static volatile int g_alarm_hits = 0;

/** @brief 解析 BLE 收到的配网信息并连接
 *         载荷格式: [ssid_len:1][pass_len:1][user_len:1][ssid][pass][username]
 *         username 保存到 NVS 以供后续向服务器申请序列号 */
static void wifi_creds_from_ble(const uint8_t *data, size_t len)
{
    size_t ssid_len, pass_len, user_len;

    if (len < 3) {
        ESP_LOGW(TAG, "wifi creds: too short (%d bytes)", (int)len);
        return;
    }
    ssid_len = data[0];
    pass_len = data[1];
    user_len = data[2];
    if (3 + ssid_len + pass_len + user_len != len) {
        ESP_LOGW(TAG, "wifi creds: length mismatch");
        return;
    }
    if (ssid_len == 0 || ssid_len > WIFI_MAX_SSID_LEN ||
        pass_len > WIFI_MAX_PASS_LEN || user_len == 0 ||
        user_len > DEV_USERNAME_MAX) {
        ESP_LOGW(TAG, "wifi creds: invalid len ssid=%d pass=%d user=%d",
                 (int)ssid_len, (int)pass_len, (int)user_len);
        return;
    }

    char ssid[WIFI_MAX_SSID_LEN + 1] = {0};
    char pass[WIFI_MAX_PASS_LEN + 1] = {0};
    char user[DEV_USERNAME_MAX + 1]  = {0};

    memcpy(ssid, data + 3, ssid_len);
    memcpy(pass, data + 3 + ssid_len, pass_len);
    memcpy(user, data + 3 + ssid_len + pass_len, user_len);

    dev_cfg_save_username(user);

    ESP_LOGI(TAG, "wifi creds: ssid=%s pass_len=%d user=%s",
             ssid, (int)pass_len, user);
    wifi_sta_connect(ssid, pass);
}

/** @brief BLE 数据回调: 尝试解析为 WiFi 凭据 */
static void on_ble_data(const uint8_t *data, size_t len)
{
    wifi_creds_from_ble(data, len);
}

/** @brief 数值圆整为十分位并钳位 */
static int to_tenths(float x)
{
    int v = (int)(x * 10.0f + 0.5f);

    /* 允许负值(温度可为负); 显示层会钳位到 0 */
    if (v > 9999) {
        v = 9999;
    }
    return v;
}

static bool alarm_active(void);

/** @brief 传感器任务: 常驻 core 1, 周期性调用各传感器读取函数 */
static void sensor_task(void *arg)
{
    ESP_LOGI(TAG, "sensor format: (temp,flow,ec,turb)");

    while (1) {
        float temp = 0.0f, flow = 0.0f, ec = 0.0f, turb = 0.0f;
        bool temp_ok = false, flow_ok = false, ec_ok = false, turb_ok = false;

        /* 1) 温度: 启动转换并等待完成 */
        if (ds18b20_start_conversion() == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(CONVERSION_DELAY_MS));
            temp_ok = (ds18b20_read_temp(&temp) == ESP_OK);
            g_temp = temp_ok ? to_tenths(temp) : ERR_DISP;
        } else {
            g_temp = ERR_DISP;
            vTaskDelay(pdMS_TO_TICKS(CONVERSION_DELAY_MS)); /* 保持采样节奏 */
        }

        /* 2) 电导率: 无温度补偿, 整数 μS/cm */
        ec_ok = (ec_read(&ec) == ESP_OK);
        if (ec_ok) {
            int v = (int)(ec + 0.5f);
            g_ec = (v < 0) ? 0 : ((v > 9999) ? 9999 : v);
        } else {
            g_ec = ERR_DISP;
        }

        /* 3) 浊度: 简易 0~100 指数 */
        turb_ok = (turb_read(&turb) == ESP_OK);
        if (turb_ok) {
            int v = (int)(turb + 0.5f);
            g_turb = (v < 0) ? 0 : ((v > 100) ? 100 : v);
        } else {
            g_turb = ERR_DISP;
        }

        /* 4) 流量: 读取一个测量窗口内的流量 */
        flow_ok = (yf_s201_read_flow(&flow) == ESP_OK);
        g_flow = flow_ok ? to_tenths(flow) : ERR_DISP;

        /* 每次采样一行输出 */
        char s_temp[16], s_flow[16], s_ec[16], s_turb[16];
        if (temp_ok) {
            snprintf(s_temp, sizeof s_temp, "%.1f", temp);
        } else {
            strcpy(s_temp, "ERR");
        }
        if (flow_ok) {
            snprintf(s_flow, sizeof s_flow, "%.2f", flow);
        } else {
            strcpy(s_flow, "ERR");
        }
        if (ec_ok) {
            snprintf(s_ec, sizeof s_ec, "%.0f", ec);
        } else {
            strcpy(s_ec, "ERR");
        }
        if (turb_ok) {
            snprintf(s_turb, sizeof s_turb, "%.0f", turb);
        } else {
            strcpy(s_turb, "ERR");
        }
        ESP_LOGI(TAG, "sensor(%s,%s,%s,%s)", s_temp, s_flow, s_ec, s_turb);

        /* 以最近一次采样判断报警状态, 统计连续命中次数供报警任务去抖 */
        g_alarm_hits = alarm_active() ? (g_alarm_hits + 1) : 0;

        vTaskDelay(pdMS_TO_TICKS(HOLD_DELAY_MS));
    }
}

/** @brief 按键边沿检测: 消抖 5 次采样后返回 true(每按一次只触发一次) */
static bool key_scan_edge(int key)
{
    static bool handled[KEY_NUM] = { false };
    static int  debounce[KEY_NUM] = { 0 };

    if (is_key_down(key)) {
        if (debounce[key] < 5) {
            debounce[key]++;
        }
        if (debounce[key] == 5 && !handled[key]) {
            handled[key] = true;
            return true;
        }
    } else {
        debounce[key] = 0;
        handled[key] = false;
    }
    return false;
}

/** @brief 显示共享值, 错误时显示 EEEE; point 为小数点位置(2=XX.X, -1=整数) */
static void display_value(int v, int point)
{
    if (v == ERR_DISP) {
        set_seg(0xE, 0xE, 0xE, 0xE); /* EEEE */
        set_point(-1);
        return;
    }

    if (v < 0) {
        v = 0;
    }
    if (v > 9999) {
        v = 9999;
    }
    set_num(v);     /* 例: 25.5 -> 255 -> "025.5" */
    set_point(point);
}

/** @brief 控制任务: 常驻 core 0, 按键扫描 + 显示切换 + 蓝牙开关 + 数码管/LED 扫描 */
static void control_task(void *arg)
{
    int sel = SEL_TEMP;
    int last_v = -2; /* 与初始值不同, 确保首轮刷新 */
    bool ble_on = false;
    uint8_t last_led = 0xFF; /* 与初始不同, 确保首轮写入 */

    while (1) {
        if (key_scan_edge(KEY_0)) {
            sel = (sel + 1) % SEL_NUM;
            last_v = -2; /* 强制刷新显示 */
        }

        if (key_scan_edge(KEY_1)) {
            ble_on = !ble_on;
            if (ble_on) {
                wm_ble_start();
            } else {
                wm_ble_stop();
            }
        }

        /* 组装 LED 指示: 当前显示项 + 蓝牙指示 + WiFi 状态(LED6, 常亮=已连接) */
        uint8_t led_mask = SEL_LED[sel] | (ble_on ? LED_BLE : 0) |
                           (wifi_sta_is_connected() ? LED_WIFI : 0);
        if (led_mask != last_led) {
            set_led(led_mask);
            last_led = led_mask;
        }

        int v;
        switch (sel) {
        case SEL_FLOW:
            v = g_flow;
            break;
        case SEL_EC:
            v = g_ec;
            break;
        case SEL_TURB:
            v = g_turb;
            break;
        case SEL_TEMP:
        default:
            v = g_temp;
            break;
        }

        if (v != last_v) {
            display_value(v, SEL_POINT[sel]);
            last_v = v;
        }

        seg_refresh();
        led_refresh();
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

/** @brief 将共享采样值转换为浮点, 错误值时返回 0 */
static float shared_to_float(int tenths)
{
    if (tenths == ERR_DISP) {
        return 0.0f;
    }
    if (tenths < 0) {
        tenths = 0;
    }
    return tenths / 10.0f;
}

/** @brief 上报任务: 常驻 core 1, 每隔 1s 将最新传感器数据 POST 到服务器 */
static void upload_task(void *arg)
{
    while (1) {
        if (wifi_sta_is_connected()) {
            float temp = shared_to_float(g_temp);
            float flow = shared_to_float(g_flow);
            float turb = (g_turb == ERR_DISP) ? 0.0f : (float)g_turb;
            int   ec   = (g_ec > 0) ? g_ec : 0;
            wm_http_upload(PH_VALUE, temp, flow, turb, ec);
        }
        vTaskDelay(pdMS_TO_TICKS(UPLOAD_PERIOD_MS));
    }
}

/** @brief 判断当前传感器数据是否超出阈值(任一超限即报警) */
static bool alarm_active(void)
{
    /* 温度: 读取失败(ERR_DISP)不参与判断; 实际温度 = g_temp/10 */
    bool temp_alarm = (g_temp != ERR_DISP) &&
        ((float)g_temp / 10.0f <= TEMP_ALARM_LOW_C ||
         (float)g_temp / 10.0f >  TEMP_ALARM_HIGH_C);

    /* 流量: 实际流量 = g_flow/10 (L/min) */
    bool flow_alarm = (g_flow != ERR_DISP) &&
        ((float)g_flow / 10.0f > FLOW_ALARM_HIGH_LPM);

    /* 电导率: g_ec 已是整数 μS/cm */
    bool ec_alarm = (g_ec != ERR_DISP) &&
        ((float)g_ec > EC_ALARM_HIGH_US_CM);

    /* 浊度: g_turb 已是整数 0~100 */
    bool turb_alarm = (g_turb != ERR_DISP) &&
        ((float)g_turb > TURB_ALARM_HIGH_NTU);

    return temp_alarm || flow_alarm || ec_alarm || turb_alarm;
}

/** @brief 报警明细: 取第一个超限项的类型/读数/阈值(优先级: 温度>流量>电导率>浊度) */
typedef struct {
    const char *type;
    float value;
    float threshold;
} alarm_detail_t;

static alarm_detail_t alarm_detail(void)
{
    float t = (float)g_temp / 10.0f;
    float f = (float)g_flow / 10.0f;

    if (g_temp != ERR_DISP && (t <= TEMP_ALARM_LOW_C || t > TEMP_ALARM_HIGH_C)) {
        if (t <= TEMP_ALARM_LOW_C) {
            return (alarm_detail_t){ "temperature", t, TEMP_ALARM_LOW_C };
        }
        return (alarm_detail_t){ "temperature", t, TEMP_ALARM_HIGH_C };
    }
    if (g_flow != ERR_DISP && f > FLOW_ALARM_HIGH_LPM) {
        return (alarm_detail_t){ "flow", f, FLOW_ALARM_HIGH_LPM };
    }
    if (g_ec != ERR_DISP && (float)g_ec > EC_ALARM_HIGH_US_CM) {
        return (alarm_detail_t){ "ec", (float)g_ec, EC_ALARM_HIGH_US_CM };
    }
    if (g_turb != ERR_DISP && (float)g_turb > TURB_ALARM_HIGH_NTU) {
        return (alarm_detail_t){ "turbidity", (float)g_turb, TURB_ALARM_HIGH_NTU };
    }
    return (alarm_detail_t){ "temperature", t, TEMP_ALARM_HIGH_C };
}

static void build_alarm_message(char *buf, size_t size, const alarm_detail_t *d)
{
    if (!strcmp(d->type, "temperature")) {
        snprintf(buf, size, "温度超标：%.1f℃（阈值%.1f℃）", d->value, d->threshold);
    } else if (!strcmp(d->type, "flow")) {
        snprintf(buf, size, "流量超标：%.2f L/min（阈值%.1f L/min）", d->value, d->threshold);
    } else if (!strcmp(d->type, "ec")) {
        snprintf(buf, size, "电导率超标：%.0f μS/cm（阈值%.1f μS/cm）", d->value, d->threshold);
    } else {
        snprintf(buf, size, "浊度超标：%.0f（阈值%.1f）", d->value, d->threshold);
    }
}

/** @brief 报警任务: 超阈值时蜂鸣器"响 500ms / 停 500ms"循环, 并在报警上升沿上报一条 */
static void alarm_task(void *arg)
{
    bool was_alarm = false;

    while (1) {
        bool now_alarm = (g_alarm_hits >= ALARM_DEBOUNCE_SAMPLES);

        /* 边沿: 无报警 -> 有报警 时上报一条 (同一报警周期只报一次) */
        if (now_alarm && !was_alarm) {
            char serial[DEV_SERIAL_MAX] = {0};
            dev_cfg_get_serial(serial, sizeof serial);
            if (serial[0]) {
                alarm_detail_t d = alarm_detail();
                char msg[128];
                build_alarm_message(msg, sizeof msg, &d);
                wm_http_report_alarm(serial, d.type, d.value, d.threshold,
                                     PH_VALUE, (float)g_temp / 10.0f,
                                     (float)g_flow / 10.0f,
                                     (float)g_turb, (g_ec > 0 ? g_ec : 0), msg);
            } else {
                ESP_LOGW(TAG, "alarm: no serial, skip server report");
            }
        }
        was_alarm = now_alarm;

        if (now_alarm) {
            ESP_LOGW(TAG, "!! ALARM T=%.1f F=%.1f EC=%d TURB=%d",
                     (float)g_temp / 10.0f, (float)g_flow / 10.0f,
                     g_ec, g_turb);
            start_beep();
            vTaskDelay(pdMS_TO_TICKS(BEEP_ON_MS));
            stop_beep();
            vTaskDelay(pdMS_TO_TICKS(BEEP_OFF_MS));
        } else {
            vTaskDelay(pdMS_TO_TICKS(ALARM_POLL_MS));
        }
    }
}

/** @brief 设备绑定任务: 等待 WiFi 连上后向服务器申请序列号并存到 NVS */
static void bind_task(void *arg)
{
    while (1) {
        if (wifi_sta_is_connected()) {
            if (dev_cfg_has_serial()) {
                vTaskDelay(pdMS_TO_TICKS(5000));
                continue;
            }
            char username[DEV_USERNAME_MAX + 1] = {0};
            esp_err_t gerr = dev_cfg_get_username(username, sizeof username);
            if (gerr != ESP_OK || username[0] == '\0') {
                ESP_LOGW(TAG, "no username in NVS (%s), use BLE to provision",
                         esp_err_to_name(gerr));
            } else {
                char serial[DEV_SERIAL_MAX] = {0};
                esp_err_t err = wm_http_fetch_serial(username, serial,
                                                     sizeof serial);
                if (err == ESP_OK && serial[0]) {
                    dev_cfg_save_serial(serial);
                    ESP_LOGI(TAG, "device bound, serial=%s", serial);
                } else {
                    ESP_LOGW(TAG,
                             "serial fetch failed for user=%s (%s), retry in 5s",
                             username, esp_err_to_name(err));
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main(void)
{
    seg_init();
    led_init();
    key_init();
    beep_init();
    ds18b20_init(DS18B20_PIN);
    yf_s201_init(YF_S201_PIN);
    ec_init(EC_PIN);
    turb_init();

    set_led(LED_TEMP); /* 默认指示温度 */
    display_value(0, SEL_POINT[SEL_TEMP]); /* 开机显示 0.0 */

    wm_ble_init();
    wm_ble_set_data_cb(on_ble_data);

    wifi_sta_init();

    xTaskCreatePinnedToCore(sensor_task, "sensor", 4096, NULL, 4, NULL, 1);
    xTaskCreatePinnedToCore(control_task, "control", 2048, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(upload_task, "upload", 4096, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(alarm_task, "alarm", ALARM_TASK_STACK, NULL,
                            ALARM_TASK_PRIO, NULL, ALARM_TASK_CORE);
    xTaskCreatePinnedToCore(bind_task, "bind", 4096, NULL, 3, NULL, 1);
}
