/**
 * @file seg.h
 * @brief 4 位 7 段共阴数码管驱动(74HC138 位选译码 + GPIO 段选)
 *
 * 硬件连接:
 *   - 位选: 74HC138 A0 -> IO11, A1 -> IO12, A2 接地(只用 Y0~Y3)
 *           Y0 -> G1(第1位), Y1 -> G2(第2位), Y2 -> G3(第3位), Y3 -> G4(第4位)
 *   - 段选: 共阴数码管, 段码引脚为高电平点亮
 *           a -> IO18, b -> IO16, c -> IO9,  d -> IO3
 *           e -> IO8,  f -> IO17, g -> IO10, dp -> IO46
 *
 * 译码表(段码, bit0=a, bit1=b, bit2=c, bit3=d, bit4=e, bit5=f, bit6=g, bit7=dp):
 *   索引 0~9 对应数字 0~9, 索引 10~15 对应 A~F
 *   | 索引 | 显示 | 段码 |   | 索引 | 显示 | 段码 |
 *   |  0   |  0   | 0x3F |   |  8   |  8   | 0x7F |
 *   |  1   |  1   | 0x06 |   |  9   |  9   | 0x6F |
 *   |  2   |  2   | 0x5B |   | 10   |  A   | 0x77 |
 *   |  3   |  3   | 0x4F |   | 11   |  b   | 0x7C |
 *   |  4   |  4   | 0x66 |   | 12   |  C   | 0x39 |
 *   |  5   |  5   | 0x6D |   | 13   |  d   | 0x5E |
 *   |  6   |  6   | 0x7D |   | 14   |  E   | 0x79 |
 *   |  7   |  7   | 0x07 |   | 15   |  F   | 0x71 |
 *
 * 原理说明:
 *   4 位采用动态扫描(轮询刷新), 同一时刻只有一位被点亮,
 *   seg_refresh() 每次调用切换到下一位, 使用方必须周期性调用。
 *   切换位时先熄灭段码再换位, 以避免残影。
 *
 * 注意:
 *   digit 0~3 与 G1~G4 的对应关系假设 G1 为最左侧数码管,
 *   若实际板上方向相反, 只需交换 set_seg 的参数顺序。
 *
 * 使用:
 *   1. seg_init();
 *   2. set_num(1234);            // 显示 1234
 *   3. set_point(2);             // 第3位显示小数点 -> 123.4
 *   4. set_seg(0xA, 0xb, 0xC, 0xd); // 按译码表索引显示 AbCd
 *   5. 循环中周期调用 seg_refresh();
 */

#ifndef __SEG_H__
#define __SEG_H__

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SEG_DIG_A0_PIN   GPIO_NUM_11
#define SEG_DIG_A1_PIN   GPIO_NUM_12

/**
 * @brief 初始化数码管引脚(位选 2 根 + 段选 8 根全部配置为推挽输出)
 * @note  初始化后所有位显示空, 无小数点
 */
void seg_init(void);

/**
 * @brief 显示一个数字 (0 ~ 9999)
 * @param num 要显示的数字, 超出 0~9999 范围会被钳位
 * @note  内部按千/百/十/个位填充 4 位译码表索引, 并清除所有小数点
 */
void set_num(int num);

/**
 * @brief 按译码表索引直接设置 4 位显示内容
 * @param s0 第 1 位(最左)译码表索引 (0~15)
 * @param s1 第 2 位译码表索引 (0~15)
 * @param s2 第 3 位译码表索引 (0~15)
 * @param s3 第 4 位(最右)译码表索引 (0~15)
 * @note  索引 0~9 显示数字 0~9, 10~15 显示 A~F; 越界自动钳位
 */
void set_seg(int s0, int s1, int s2, int s3);

/**
 * @brief 设置某一位的小数点
 * @param digit 小数点所在位: 0~3 对应第 1~4 位
 * @note  传入 0~3 时在该位点亮小数点、其余位熄灭;
 *        传入其他值(如 -1)时清除全部小数点
 */
void set_point(int digit);

/**
 * @brief 数码管刷新函数 (必须周期性调用)
 * @note  每调用一次切换显示下一位, 建议在主循环中与 led_refresh() 交替调用
 */
void seg_refresh(void);

#ifdef __cplusplus
}
#endif

#endif /* __SEG_H__ */
