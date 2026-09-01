/**
 * @file ec.c
 * @brief 悦常盛电导率传感器驱动实现
 *
 * 通过 ADC 读取模块输出电压, 按电导率电压曲线换算, 无温度补偿。
 */

#include "ec.h"

#include "driver/rtc_io.h"
#include "adc1_common.h"

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
    return ESP_OK;
}
