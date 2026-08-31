/**
 * @file ec.h
 * @brief 悦常盛(兼容 DFRobot Gravity SEN0244)电导率/EC 传感器驱动
 *
 * 硬件连接:
 *   - 模块模拟输出 A -> GPIO4 (ESP32-S3 ADC1_CH3)
 *   - 模块 VCC -> 3.3V(或按模块要求 5V, 与 ESP32 共地), GND -> GND
 *   - 模块输出为 0~2.3V 模拟电压, 量级约 0~2000 μS/cm
 *
 * 换算(电压 -> 电导率, 不做温度补偿):
 *   EC(μS/cm) = (133.42*V^3 - 255.86*V^2 + 857.39*V) * EC_CAL
 *
 * 与 TDS 的关系: EC(μS/cm) ≈ TDS(mg/L) / 0.5, 故本模块不再乘 TDS 换算系数。
 * 曲线可用标准电导率标定液微调 EC_CAL。
 *
 * 使用:
 *   1. ec_init(GPIO_NUM_4);
 *   2. ec_read(&ec);  // 返回 EC(μS/cm)
 */

#ifndef __EC_H__
#define __EC_H__

#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EC_PIN GPIO_NUM_4
#define EC_CAL 1.0f /**< 标定系数(默认 1.0, 标定时微调) */

/** @brief 初始化 ADC 通道(一次单元 + eFuse 校准) */
void ec_init(gpio_num_t pin);

/**
 * @brief 读取电导率
 * @param ec 输出的电导率(μS/cm)
 * @return ESP_OK 成功
 * @note  内部多次采样取平均以抑制噪声
 */
esp_err_t ec_read(float *ec);

#ifdef __cplusplus
}
#endif

#endif /* __EC_H__ */
