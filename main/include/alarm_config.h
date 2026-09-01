/**
 * @file alarm_config.h
 * @brief 阈值报警系统——参数声明文件
 *
 * 本文件集中定义阈值报警所需的全部可调参数:
 *   - 各项传感器报警阈值(超限即报警)
 *   - 报警鸣响节奏(每周期鸣响/静音时长)
 *   - 报警任务调度参数(优先级 / 核心 / 栈大小)
 *
 * 修改此文件即可调整报警行为, 无需改动报警任务代码。
 * 所有阈值单位取自传感器原始物理量, 与显示/上报一致。
 *
 * 说明: 判断采用"超过阈值即报警"; 温度采用"上/下双阈值区间判断"。
 */

#ifndef __ALARM_CONFIG_H__
#define __ALARM_CONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif

/* ====================== 1. 温度阈值 ====================== */
/*
 * 温度判断: 温度 <= TEMP_ALARM_LOW_C  或  温度 > TEMP_ALARM_HIGH_C 时报警。
 * 即以 (TEMP_ALARM_LOW_C, TEMP_ALARM_HIGH_C] 作为正常区间, 区间外报警。
 * 单位: ℃
 */
#define TEMP_ALARM_LOW_C     0.0f   /**< 温度下限: 小于等于此值时报警(含 0℃) */
#define TEMP_ALARM_HIGH_C   50.0f   /**< 温度上限: 大于此值时报警(含 50℃ 以下的都是正常) */

/* ====================== 2. 水流量阈值 ====================== */
/*
 * 流量判断: 流量 > FLOW_ALARM_HIGH_LPM 时报警。
 * 单位: L/min (流量传感器单位为升/分钟)
 */
#define FLOW_ALARM_HIGH_LPM 300.0f  /**< 流量上限: 大于此值时报警 */

/* ====================== 3. 电导率阈值 ====================== */
/*
 * 电导率判断: 电导率 > EC_ALARM_HIGH_US_CM 时报警。
 * 单位: μS/cm (微西门子每厘米)
 */
#define EC_ALARM_HIGH_US_CM 500.0f  /**< 电导率上限: 大于此值时报警 */

/* ====================== 4. 浊度阈值 ====================== */
/*
 * 浊度判断: 浊度 > TURB_ALARM_HIGH_NTU 时报警。
 * 单位: NTU
 */
#define TURB_ALARM_HIGH_NTU 40.0f   /**< 浊度上限: 大于此值时报警 */

/* ====================== 5. 报警鸣响节奏 ====================== */
/*
 * 进入报警状态后, 蜂鸣器按"鸣响 BEEP_ON_MS 毫秒 + 静音 BEEP_OFF_MS 毫秒"
 * 循环, 形成周期约为 (BEEP_ON_MS + BEEP_OFF_MS) 的间断报警音。
 * 默认 500ms 响 500ms 停, 即每秒报警一次。
 */
#define BEEP_ON_MS    500           /**< 每次鸣响时长(毫秒) */
#define BEEP_OFF_MS   500           /**< 每次静音时长(毫秒) */

/* 非报警状态下, 每隔该时长重新检测一次阈值, 以便报警快速触发/解除 */
#define ALARM_POLL_MS 100           /**< 非报警时的轮询周期(毫秒) */

/* ====================== 6. 报警任务调度 ====================== */
/*
 * 报警任务为常驻 FreeRTOS 任务, 参数如下:
 *   - ALARM_TASK_PRIO : 任务优先级(数值越大优先级越高)
 *   - ALARM_TASK_CORE : 绑定运行的核心(0 或 1)
 *   - ALARM_TASK_STACK: 任务栈大小(字节)
 */
#define ALARM_TASK_PRIO  3          /**< 报警任务优先级 */
#define ALARM_TASK_CORE  0          /**< 报警任务绑定核心(0/1) */
#define ALARM_TASK_STACK 4096       /**< 报警任务栈大小(字节) */

#ifdef __cplusplus
}
#endif

#endif /* __ALARM_CONFIG_H__ */
