/**
 * @file adc1_common.h
 * @brief ESP32-S3 ADC1 共享访问层
 *
 * ESP-IDF 每个 ADC 单元只允许一个 oneshot handle, 而 EC(GPIO4/CH3) 与
 * 浊度(GPIO7/CH6) 都需用 ADC1。本层统一创建 ADC1 句柄并管理各通道
 * 配置与 eFuse 校准, 供多个模拟传感器模块共用。
 */

#ifndef __ADC1_COMMON_H__
#define __ADC1_COMMON_H__

#include "esp_err.h"
#include "driver/gpio.h"
#include "hal/adc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 创建 ADC1 oneshot 句柄(幂等, 重复调用直接返回 OK) */
esp_err_t adc1_common_init(void);

/**
 * @brief 配置一个 ADC1 通道(12dB/12bit + eFuse 校准)
 * @param pin         GPIO 编号(须为 ADC1 引脚, 如 GPIO4/GPIO7)
 * @param out_channel 输出的通道号, 供读取时使用
 * @return ESP_OK 成功; ESP_ERR_NOT_FOUND 无此 ADC 通道; ESP_ERR_INVALID_ARG 非 ADC1
 */
esp_err_t adc1_common_add_channel(gpio_num_t pin, adc_channel_t *out_channel);

/**
 * @brief 读取某通道电压
 * @param channel    通道号(由 adc1_common_add_channel 获得)
 * @param voltage_mv 输出电压(mV)
 * @return ESP_OK 成功
 * @note  内部多次采样取平均以抑制噪声
 */
esp_err_t adc1_common_read_mv(adc_channel_t channel, int *voltage_mv);

#ifdef __cplusplus
}
#endif

#endif /* __ADC1_COMMON_H__ */
