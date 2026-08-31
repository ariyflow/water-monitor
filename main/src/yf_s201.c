/**
 * @file yf_s201.c
 * @brief YF-S201 水流量传感器驱动实现
 *
 * 通过 GPIO 上升沿中断累计脉冲数, 按两次读取的时间间隔计算频率并换算流量。
 */

#include "yf_s201.h"

#include "esp_attr.h"
#include "esp_timer.h"

static volatile uint32_t s_pulse_count;  /**< 上升沿中断累计脉冲数 */
static uint32_t          s_last_count;   /**< 上次读取时的脉冲计数 */
static int64_t           s_last_time_us; /**< 上次读取时刻(us) */

/** @brief 上升沿中断: 仅累计脉冲数; 放 IRAM 以兼容 cache 关闭场景 */
static void IRAM_ATTR yf_s201_isr(void *arg)
{
    (void)arg;
    s_pulse_count++;
}

void yf_s201_init(gpio_num_t pin)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE,
    };
    gpio_config(&cfg);

    /* 中断服务仅安装一次, 已安装则忽略(兼容其他模块共用) */
    esp_err_t err = gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1 |
                                             ESP_INTR_FLAG_IRAM);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return;
    }
    gpio_isr_handler_add(pin, yf_s201_isr, NULL);

    s_pulse_count = 0;
    s_last_count = 0;
    s_last_time_us = esp_timer_get_time();
}

esp_err_t yf_s201_read_flow(float *flow_lmin)
{
    uint32_t cur = s_pulse_count;
    int64_t  now = esp_timer_get_time();
    uint32_t delta = cur - s_last_count;               /* 窗口内脉冲数 */
    float    dt = (float)(now - s_last_time_us) / 1000000.0f;

    s_last_count = cur;
    s_last_time_us = now;

    if (dt <= 0.0f) {
        *flow_lmin = 0.0f;
        return ESP_OK;
    }

    float freq = (float)delta / dt;    /* Hz */
    *flow_lmin = freq / YF_S201_K;     /* L/min */
    return ESP_OK;
}
