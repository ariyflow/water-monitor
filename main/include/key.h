/**
 * @file key.h
 * @brief 4 路独立按键驱动
 *
 * 硬件连接(按键按下时引脚被拉低, 释放时恢复高电平):
 *   - 按键0 -> IO48
 *   - 按键1 -> IO45
 *   - 按键2 -> IO21
 *   - 按键3 -> IO47
 *
 * 说明:
 *   - 按键使用内部上拉电阻, 按下 = 低电平 (KEY_ACTIVE_LEVEL 可调)
 *   - is_key_down() 为"轮询读取"接口, 返回按键当前电平状态;
 *     它不做软件消抖, 若需要消抖请在使用方配合延时判断
 *     (参考测试程序 main.c 中的按键扫描方式)
 *
 * 使用:
 *   1. key_init();
 *   2. if (is_key_down(KEY_0)) { ... }   // 按键0 被按下时返回 true
 */

#ifndef __KEY_H__
#define __KEY_H__

#include <stdbool.h>
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 按键索引 */
#define KEY_0   0
#define KEY_1   1
#define KEY_2   2
#define KEY_3   3
#define KEY_NUM 4

/** 按键按下时的有效电平 (0 = 按下为低电平) */
#define KEY_ACTIVE_LEVEL   0

/**
 * @brief 初始化 4 路按键引脚(输入模式 + 内部上拉)
 */
void key_init(void);

/**
 * @brief 检测指定按键是否处于按下状态
 * @param key 按键索引 (KEY_0 ~ KEY_3)
 * @return true  = 当前按下
 * @return false = 当前未按下(或参数越界)
 */
bool is_key_down(int key);

#ifdef __cplusplus
}
#endif

#endif /* __KEY_H__ */
