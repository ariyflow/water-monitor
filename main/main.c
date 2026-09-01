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

#define TAG "sensor_disp"

#define CONVERSION_DELAY_MS 750 /**< DS18B20 12bit 转换时间 */
#define HOLD_DELAY_MS       250 /**< 传感器两次采样间间隔 */

#define ERR_DISP -1 /**< 共享值中表示读取失败的标志 */

#define LED_TEMP 0x01 /**< 指示位: 数码管显示温度 */
#define LED_FLOW 0x02 /**< 指示位: 数码管显示水流量 */
#define LED_EC   0x04 /**< 指示位: 数码管显示电导率 */
#define LED_TURB 0x08 /**< 指示位: 数码管显示浊度 */
#define LED_BLE  0x80 /**< 指示位: 蓝牙开启指示(LED7, 最高位) */

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

/** @brief 解析 BLE 收到的 WiFi 凭据并连接
 *         载荷格式: [ssid_len:1][pass_len:1][ssid][pass] */
static void wifi_creds_from_ble(const uint8_t *data, size_t len)
{
    size_t ssid_len, pass_len;

    if (len < 2) {
        ESP_LOGW(TAG, "wifi creds: too short (%d bytes)", (int)len);
        return;
    }
    ssid_len = data[0];
    pass_len = data[1];
    if (2 + ssid_len + pass_len != len) {
        ESP_LOGW(TAG, "wifi creds: length mismatch");
        return;
    }
    if (ssid_len == 0 || ssid_len > WIFI_MAX_SSID_LEN ||
        pass_len > WIFI_MAX_PASS_LEN) {
        ESP_LOGW(TAG, "wifi creds: invalid len ssid=%d pass=%d",
                 (int)ssid_len, (int)pass_len);
        return;
    }

    char ssid[WIFI_MAX_SSID_LEN + 1] = {0};
    char pass[WIFI_MAX_PASS_LEN + 1] = {0};

    memcpy(ssid, data + 2, ssid_len);
    memcpy(pass, data + 2 + ssid_len, pass_len);

    ESP_LOGI(TAG, "wifi creds: ssid=%s pass_len=%d", ssid, (int)pass_len);
    wifi_sta_connect(ssid, pass);
}

/** @brief BLE 数据回调: 打印收到内容, 并尝试解析为 WiFi 凭据 */
static void on_ble_data(const uint8_t *data, size_t len)
{
    ESP_LOGI(TAG, "ble rx %d bytes: %.*s", (int)len, (int)len,
             (const char *)data);
    wifi_creds_from_ble(data, len);
}

/** @brief 数值圆整为十分位并钳位 */
static int to_tenths(float x)
{
    int v = (int)(x * 10.0f + 0.5f);

    if (v < 0) {
        v = 0;
    }
    if (v > 9999) {
        v = 9999;
    }
    return v;
}

/** @brief 传感器任务: 常驻 core 1, 周期性调用各传感器读取函数 */
static void sensor_task(void *arg)
{
    while (1) {
        /* 1) 温度: 启动转换并等待完成 */
        if (ds18b20_start_conversion() == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(CONVERSION_DELAY_MS));

            float temp;
            if (ds18b20_read_temp(&temp) == ESP_OK) {
                g_temp = to_tenths(temp);
                ESP_LOGI(TAG, "temperature: %.2f C", temp);
            } else {
                g_temp = ERR_DISP;
                ESP_LOGE(TAG, "failed to read temperature");
            }
        } else {
            g_temp = ERR_DISP;
            ESP_LOGE(TAG, "ds18b20 not found on GPIO%d", DS18B20_PIN);
            vTaskDelay(pdMS_TO_TICKS(CONVERSION_DELAY_MS)); /* 保持采样节奏 */
        }

        /* 2) 电导率: 无温度补偿, 整数 μS/cm */
        float ec;
        if (ec_read(&ec) == ESP_OK) {
            int v = (int)(ec + 0.5f);
            if (v < 0) {
                v = 0;
            }
            if (v > 9999) {
                v = 9999;
            }
            g_ec = v;
            ESP_LOGI(TAG, "ec: %.0f uS/cm", ec);
        } else {
            g_ec = ERR_DISP;
            ESP_LOGE(TAG, "failed to read ec");
        }

        /* 3) 浊度: 简易 0~100 指数 */
        float turb;
        if (turb_read(&turb) == ESP_OK) {
            int v = (int)(turb + 0.5f);
            if (v < 0) {
                v = 0;
            }
            if (v > 100) {
                v = 100;
            }
            g_turb = v;
            ESP_LOGI(TAG, "turbidity: %.0f", turb);
        } else {
            g_turb = ERR_DISP;
            ESP_LOGE(TAG, "failed to read turbidity");
        }

        /* 4) 流量: 读取一个测量窗口内的流量 */
        float flow;
        if (yf_s201_read_flow(&flow) == ESP_OK) {
            g_flow = to_tenths(flow);
            ESP_LOGI(TAG, "flow: %.2f L/min", flow);
        } else {
            g_flow = ERR_DISP;
        }

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

    while (1) {
        if (key_scan_edge(KEY_0)) {
            sel = (sel + 1) % SEL_NUM;
            set_led(SEL_LED[sel] | (ble_on ? LED_BLE : 0));
            last_v = -2; /* 强制刷新显示 */
        }

        if (key_scan_edge(KEY_1)) {
            ble_on = !ble_on;
            if (ble_on) {
                wm_ble_start();
            } else {
                wm_ble_stop();
            }
            set_led(SEL_LED[sel] | (ble_on ? LED_BLE : 0));
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

void app_main(void)
{
    seg_init();
    led_init();
    key_init();
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
}
