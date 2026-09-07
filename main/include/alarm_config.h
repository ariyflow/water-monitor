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
#define FLOW_ALARM_HIGH_LPM 20.0f  /**< 流量上限: 大于此值时报警 */

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

/* ====================== 5. PH 阈值 ====================== */
/*
 * PH 判断: PH < PH_ALARM_LOW_PH 或 PH > PH_ALARM_HIGH_PH 时报警。
 * 即以 [PH_ALARM_LOW_PH, PH_ALARM_HIGH_PH] 作为正常区间, 区间外报警。
 * 单位: pH
 */
#define PH_ALARM_LOW_PH   4.0f   /**< PH 下限: 小于此值时报警 */
#define PH_ALARM_HIGH_PH 10.0f   /**< PH 上限: 大于此值时报警 */

/* ====================== 6. 报警鸣响节奏 ====================== */
/*
 * 进入报警状态后, 蜂鸣器按"鸣响 BEEP_ON_MS 毫秒 + 静音 BEEP_OFF_MS 毫秒"
 * 循环, 形成周期约为 (BEEP_ON_MS + BEEP_OFF_MS) 的间断报警音。
 * 默认 500ms 响 500ms 停, 即每秒报警一次。
 */
#define BEEP_ON_MS    500           /**< 每次鸣响时长(毫秒) */
#define BEEP_OFF_MS   500           /**< 每次静音时长(毫秒) */

/* 非报警状态下, 每隔该时长重新检测一次阈值, 以便报警快速触发/解除 */
#define ALARM_POLL_MS 100           /**< 非报警时的轮询周期(毫秒) */

/*
 * 报警去抖: 连续 ALARM_DEBOUNCE_SAMPLES 次采样都满足报警条件才真正鸣响。
 * 用于避免上电初期(温度初值/ADC 首采未稳定)的瞬时超限导致的误报。
 * 采样周期约 1s(见 main.c HOLD_DELAY_MS + DS18B20 转换时间), 默认 3 次≈3s。
 */
#define ALARM_DEBOUNCE_SAMPLES 3    /**< 连续满足报警条件的采样次数 */

/* ====================== 7. 报警任务调度 ====================== */
/*
 * 报警任务为常驻 FreeRTOS 任务, 参数如下:
 *   - ALARM_TASK_PRIO : 任务优先级(数值越大优先级越高)
 *   - ALARM_TASK_CORE : 绑定运行的核心(0 或 1)
 *   - ALARM_TASK_STACK: 任务栈大小(字节)
 */
#define ALARM_TASK_PRIO  3          /**< 报警任务优先级 */
#define ALARM_TASK_CORE  0          /**< 报警任务绑定核心(0/1) */
#define ALARM_TASK_STACK 4096       /**< 报警任务栈大小(字节) */

/* ====================== 8. 运行时阈值参数 ====================== */
/*
 * 上述宏为出厂默认阈值。设备上电后可向服务器拉取该设备独立的阈值
 * (GET /api/settings), 覆盖下面的运行时值; 拉取失败时沿用宏默认值。
 *
 * 每个阈值以独立的 volatile float 保存于 main.c, 单字段读/写在 Xtensa 上
 * 为单条 32bit 存取, 无撕裂。settings_task 逐字段写入, 各读任务逐字段读取,
 * 某次采样可能出现"个别字段已更新/其余未更新"的瞬时混合, 对报警判断无害。
 *
 * alarm_params_t 仅作为 HTTP 拉取函数 (wm_http_fetch_thresholds) 的输出结构,
 * 便于一次性取得全部阈值后再逐字段写入运行时变量。
 */
typedef struct {
    float temp_low_c;    /**< 温度下限(℃): 实际温度 <= 此值报警 */
    float temp_high_c;   /**< 温度上限(℃): 实际温度 > 此值报警 */
    float flow_high_lpm; /**< 流量上限(L/min): 实际流量 > 此值报警 */
    float ec_high_us_cm; /**< 电导率上限(μS/cm): 实际电导率 > 此值报警 */
    float turb_high_ntu; /**< 浊度上限(NTU): 实际浊度 > 此值报警 */
    float ph_low;        /**< PH 下限: 实际 PH < 此值报警 */
    float ph_high;       /**< PH 上限: 实际 PH > 此值报警 */
} alarm_params_t;

#ifdef __cplusplus
}
#endif

#endif /* __ALARM_CONFIG_H__ */
