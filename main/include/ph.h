/**
 * @file ph.h
 * @brief PH 模拟传感器驱动
 *
 * 硬件连接:
 *   - 模块模拟输出 AO -> GPIO9 (ESP32-S3 ADC1_CH8)
 *   - 模块 VCC -> 3.3V(模块输出 0~3.3V, 直接接入 ADC), GND 与 ESP32 共地
 *
 * 校准(线性标定, 电压随 pH 升高而下降):
 *   pH = (PH_V_ZERO - V) / PH_SLOPE
 *   三点标定: pH4.00 -> 1.73V, pH6.86 -> 1.22V, pH9.18 -> 0.85V
 *   拟合得: pH=0 截距 ~2.40V, 斜率 ~0.17 V/pH
 *
 * 使用:
 *   1. ph_init();
 *   2. ph_read(&ph);  // 返回 pH 值
 *
 * 注意: 未做温度补偿, 高精度测量需结合 DS18B20 温度修正。
 */

#ifndef __PH_H__
#define __PH_H__

#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PH_PIN    GPIO_NUM_9
#define PH_V_ZERO 2.40f /**< 电压(V)在 pH=0 时的截距(由三点标定拟合) */
#define PH_SLOPE  0.17f /**< 每 1 pH 对应的电压变化(V/pH, 绝对值) */

/** @brief 初始化 PH 通道(复用共享 ADC1 单元, ADC1_CH8) */
void ph_init(void);

/**
 * @brief 读取 pH 值
 * @param ph 输出的 pH 值(0~14 附近)
 * @return ESP_OK 成功
 * @note  内部多次采样取平均以抑制噪声
 */
esp_err_t ph_read(float *ph);

#ifdef __cplusplus
}
#endif

#endif /* __PH_H__ */
