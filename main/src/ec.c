/**
 * @file ec.c
 * @brief 悦常盛电导率传感器驱动实现
 *
 * 通过 ADC 读取模块输出电压, 按电导率电压曲线换算, 无温度补偿。
 */

#include "ec.h"

#include "esp_log.h"
#include "driver/rtc_io.h"
#include "adc1_common.h"

static const char *TAG = "ec";

static adc_channel_t s_channel;

/** @brief 电压(V) -> 电导率(μS/cm) 标准曲线 */
static float ec_voltage_to_ec(float v)
{
    float v2 = v * v;
    float v3 = v2 * v;

    return (133.42f * v3 - 255.86f * v2 + 857.39f * v) * EC_CAL;
}

void ec_init(gpio_num_t pin)
{
    /* 自检: 先把该引脚配成数字输入(内部下拉), 读一次电平。
       若读到 1 且模块已拔掉, 说明引脚被外部电路强拉到高电平
       (如 LED/电阻误接到 3.3V, 或短路), 不是 ADC 配置问题。 */
    gpio_config_t d_cfg = {
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&d_cfg);
    ESP_LOGI(TAG, "selftest: GPIO%d digital=%d (1=被外部拉高, 多半接了3.3V)",
             pin, gpio_get_level(pin));

    /* 输入引脚加内部下拉: 模块未接/未供电时避免悬空被读成近满量程。
       注意: 该引脚处于 ADC(RTC 模拟)功能时, 数字 GPIO 的上下拉不生效,
       必须用 RTC IO 的上下拉寄存器。悬空时 EC 会读 ~0 而不是很大的假值。 */
    if (rtc_gpio_is_valid_gpio(pin)) {
        rtc_gpio_pullup_dis(pin);
        rtc_gpio_pulldown_en(pin);
    }

    /* 复用共享 ADC1 层(与浊度共用同一 oneshot 句柄) */
    adc1_common_init();
    adc1_common_add_channel(pin, &s_channel);
}

esp_err_t ec_read(float *ec)
{
    int voltage_mv = 0;

    if (adc1_common_read_mv(s_channel, &voltage_mv) != ESP_OK) {
        return ESP_FAIL;
    }

    *ec = ec_voltage_to_ec((float)voltage_mv / 1000.0f);
    ESP_LOGI(TAG, "v=%dmV, ec=%.0f uS/cm", voltage_mv, *ec);
    return ESP_OK;
}
