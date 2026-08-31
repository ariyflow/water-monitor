/**
 * @file led.h
 * @brief 8 路 LED 驱动(基于 74HC138 译码器)
 *
 * 硬件连接:
 *   - 74HC138 A0 -> IO35
 *   - 74HC138 A1 -> IO36
 *   - 74HC138 A2 -> IO37
 *   - 74HC138 Y0~Y7 -> 8 个 LED 阴极(LED 阳极接 VCC)
 *   - 译码器被选中的那一路输出为低电平, 对应 LED 点亮
 *
 * 译码表(地址 -> 输出):
 *   | A2 A1 A0 | 输出 | 点亮 |
 *   |  0  0  0 | Y0   | LED0 |
 *   |  0  0  1 | Y1   | LED1 |
 *   |  ...     | ...  | ...  |
 *   |  1  1  1 | Y7   | LED7 |
 *
 * set_led 字节映射:
 *   - bit0(最低位) -> Y0 -> LED0
 *   - bit1          -> Y1 -> LED1
 *   - ...
 *   - bit7(最高位) -> Y7 -> LED7
 *
 * 原理说明:
 *   74HC138 在任意时刻只能有一个输出为低(同一时刻只能点亮一个 LED),
 *   因此当 set_led 传入多个 bit 时, 采用"轮询刷新 + 视觉暂留"实现多灯显示:
 *   led_refresh() 每次调用切换到下一个需要点亮的 LED 并保持到下次调用,
 *   使用方必须周期性调用 led_refresh(), 调用间隔即每个 LED 的停留时间。
 *
 * 注意:
 *   由于译码器始终有一路输出为低, 硬件上无法让全部 8 个 LED 同时熄灭;
 *   当显示字节为 0 时, 轮询列表为空, 刷新函数不再切换(保留上一次选中状态)。
 *
 * 使用:
 *   1. led_init();
 *   2. set_led(0x01);       // 点亮 LED0 (bit0 -> Y0)
 *   3. 循环中周期调用 led_refresh();
 */

#ifndef __LED_H__
#define __LED_H__

#include <stdint.h>
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LED_A0_PIN   GPIO_NUM_35
#define LED_A1_PIN   GPIO_NUM_36
#define LED_A2_PIN   GPIO_NUM_37

/**
 * @brief 初始化 LED 译码器控制引脚 (A0/A1/A2 配置为推挽输出)
 * @note  初始化后轮询列表为空, 译码地址指向 Y0
 */
void led_init(void);

/**
 * @brief 设置 LED 显示状态
 * @param pattern 显示字节, 每一位对应一个 LED:
 *                bit0 -> LED0, bit1 -> LED1, ..., bit7 -> LED7
 * @note  置 1 的位对应的 LED 会点亮; 内部重建轮询列表,
 *        显示效果需配合周期调用 led_refresh()
 */
void set_led(uint8_t pattern);

/**
 * @brief LED 刷新函数 (必须周期性调用)
 * @note  每调用一次, 切换到下一个需要点亮的 LED 并保持到下一次调用,
 *        调用间隔决定刷新频率。建议在主循环中与 seg_refresh() 交替调用
 */
void led_refresh(void);

#ifdef __cplusplus
}
#endif

#endif /* __LED_H__ */
