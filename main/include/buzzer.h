/**
 * @file buzzer.h
 * @brief 蜂鸣器驱动
 *
 * 硬件连接:
 *   - 有源蜂鸣器信号线 -> IO2
 *   - 高电平有效(GPIO 输出高电平时蜂鸣器鸣响)
 *
 * 使用说明:
 *   1. 调用 beep_init() 完成初始化(引脚配置为推挽输出, 默认关闭)
 *   2. 调用 start_beep() 使蜂鸣器鸣响
 *   3. 调用 stop_beep() 关闭蜂鸣器
 *
 * 示例:
 *   @code
 *   beep_init();
 *   start_beep();   // 鸣响
 *   vTaskDelay(pdMS_TO_TICKS(100));
 *   stop_beep();    // 关闭
 *   @endcode
 */

#ifndef __BUZZER_H__
#define __BUZZER_H__

#include <stdbool.h>
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 蜂鸣器控制引脚 */
#define BUZZER_PIN   GPIO_NUM_2

/** 蜂鸣器有效电平 (1 = 高电平有效) */
#define BUZZER_ACTIVE_LEVEL   1

/**
 * @brief 初始化蜂鸣器引脚
 * @note  引脚配置为推挽输出, 初始状态为关闭
 */
void beep_init(void);

/**
 * @brief 打开蜂鸣器
 */
void start_beep(void);

/**
 * @brief 关闭蜂鸣器
 */
void stop_beep(void);

#ifdef __cplusplus
}
#endif

#endif /* __BUZZER_H__ */
