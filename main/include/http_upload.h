/**
 * @file http_upload.h
 * @brief 通过 HTTP POST 将传感器数据上报到服务器
 *
 * 服务器 API: POST https://<host>:<port>/api/sensors
 * 请求体: {"serial":..., "ph":..., "temperature":...,
 *          "flow":..., "turbidity":..., "conductivity":...}
 * 设备需先经 /api/devices 绑定序列号并存到 NVS; 未绑定则跳过上报。
 */

#ifndef __HTTP_UPLOAD_H__
#define __HTTP_UPLOAD_H__

#include <stddef.h>
#include "esp_err.h"
#include "alarm_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 服务器地址 */
#define SERVER_SCHEME "https"
#define SERVER_HOST "esp.ywxk.me"
#define SERVER_PORT 443
#define SERVER_PATH "/api/sensors"
#define SERVER_DEVICE_PATH "/api/devices"
#define SERVER_ALARM_PATH "/api/alarms"
#define SERVER_SETTINGS_PATH "/api/settings"

/** 传感器上报周期(ms) */
#define UPLOAD_PERIOD_MS 1000

/** PH 值固定 */
#define PH_VALUE 7.0f

/**
 * @brief 向服务器 POST 上报一次传感器数据
 * @param ph           pH 值
 * @param temperature  温度(℃)
 * @param flow          水流量(单位随 YF-S201)
 * @param turbidity    浊度(0~100)
 * @param conductivity 电导率(μS/cm)
 * @return ESP_OK 成功; 其他为失败(含 HTTP 非 2xx)
 */
esp_err_t wm_http_upload(float ph, float temperature, float flow,
                         float turbidity, int conductivity);

/**
 * @brief 向服务器申请设备序列号并绑定到指定用户
 * @param username 配网时提供的用户名(服务器会据此匹配/绑定设备)
 * @param out      输出的序列号缓冲
 * @param out_size 缓冲大小
 * @return ESP_OK 成功并写入 out; 其他为失败(含非 2xx、解析失败)
 * @note  请求体: {"username":"..."}  -> POST /api/devices
 */
esp_err_t wm_http_fetch_serial(const char *username, char *out, size_t out_size);

/**
 * @brief 向服务器上报一条报警(设备驱动, 无需登录)
 * @param serial       设备序列号
 * @param type         报警类型: temperature/flow/ec/turbidity
 * @param value        触发报警的读数
 * @param threshold    对应阈值
 * @param ph           pH 值
 * @param temperature  温度(℃)
 * @param flow         水流量
 * @param turbidity    浊度
 * @param conductivity 电导率
 * @param message      设备端拼好的中文描述
 * @return ESP_OK 成功; 其他为失败
 * @note  载荷格式: {"serial":..,"active":true,"type":..,"value":..,
 *          "threshold":..,"ph":..,"temperature":..,"flow":..,
 *          "turbidity":..,"conductivity":..,"message":".."}
 */
esp_err_t wm_http_report_alarm(const char *serial, const char *type, float value,
                               float threshold, float ph, float temperature,
                               float flow, float turbidity, int conductivity,
                               const char *message);

/**
 * @brief 向服务器拉取该设备的报警阈值
 * @param serial 设备序列号
 * @param out    输出的阈值结构 (应非空)
 * @return ESP_OK 成功并填充 out; 其他为失败(含非 2xx、解析失败)
 * @note  请求: GET /api/settings?serial=...
 *        成功填充全部字段; 任一字段缺失时保留 out 中原值(为 NULL 时清零)。
 *        未配置时服务器返回固件默认值。
 */
esp_err_t wm_http_fetch_thresholds(const char *serial, alarm_params_t *out);

#ifdef __cplusplus
}
#endif

#endif /* __HTTP_UPLOAD_H__ */
