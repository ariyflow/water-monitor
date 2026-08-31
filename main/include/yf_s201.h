/**
 * @file yf_s201.h
 * @brief YF-S201 水流量传感器驱动(霍尔脉冲计数)
 *
 * 硬件连接:
 *   - 信号线(脉冲输出) -> GPIO6
 *   - 传感器本体通常由 5V 供电(另接), 输出为集电极开路,
 *     信号线需上拉到 3.3V (驱动已使能内部上拉, 建议加外部上拉更稳)
 *
 * 原理:
 *   输出脉冲频率 f(Hz) 与流量 Q(L/min) 成正比:
 *     Q(L/min) = f / 7.5
 *   即 7.5Hz -> 1L/min, 75Hz -> 10L/min
 *
 * 使用:
 *   1. yf_s201_init(GPIO_NUM_6);
 *   2. 周期性调用 yf_s201_read_flow(&q); // 返回 L/min
 *      测量窗口 = 两次调用之间的时间间隔, 建议调用间隔约 1s;
 *      首次调用前需等待一个窗口时间再取用结果。
 */

#ifndef __YF_S201_H__
#define __YF_S201_H__

#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define YF_S201_PIN GPIO_NUM_6
#define YF_S201_K   7.5f /**< 标定常数: Hz / (L/min) */

/** @brief 初始化脉冲引脚(输入 + 内部上拉 + 上升沿中断计数) */
void yf_s201_init(gpio_num_t pin);

/**
 * @brief 读取当前流量
 * @param flow_lmin 输出的流量(L/min)
 * @return ESP_OK 成功
 * @note  以两次调用间隔作为测量窗口; 应约每秒调用一次
 */
esp_err_t yf_s201_read_flow(float *flow_lmin);

#ifdef __cplusplus
}
#endif

#endif /* __YF_S201_H__ */
