/**
 * @file turbidity.h
 * @brief TS-300B 浊度传感器模块(简易线性浊度指数, 未标定)
 *
 * 硬件连接:
 *   - 模块模拟输出 AO -> GPIO7 (ADC1_CH6)
 *   - GPIO15 不用, DO 不接
 *   - 模块 VCC -> 5V, GND 与 ESP32 共地
 *
 * 说明:
 *   - 输出 0~100 的简易浊度指数(先跑通, 未标定):
 *       V 越高越清, 指数 = 100 * (1 - V/TURB_REF_V)
 *       空气/清水 -> 接近 0, 越浊 -> 指数越高
 *   - 注意: TS-300B 的 AO 在空气里可能 ~3.4V, 超过 ADC 12dB 满量程(~3.1V),
 *     此时 ADC 饱和读 4095, 指数为 0。若需覆盖更高范围, 需加分压电阻
 *     并调整 TURB_REF_V。真实标定需用已知浊度标准液。
 */

#ifndef __TURBIDITY_H__
#define __TURBIDITY_H__

#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TURB_PIN   GPIO_NUM_7
#define TURB_REF_V 3.1f /**< 参考电压(V): 视为"最清"(指数=0)的电压 */

/** @brief 初始化浊度通道(ADC1_CH6) */
void turb_init(void);

/**
 * @brief 读取浊度指数
 * @param index 输出 0~100 (0=清, 100=极浊)
 * @return ESP_OK 成功
 */
esp_err_t turb_read(float *index);

#ifdef __cplusplus
}
#endif

#endif /* __TURBIDITY_H__ */
