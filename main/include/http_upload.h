/**
 * @file http_upload.h
 * @brief 通过 HTTP POST 将传感器数据上报到服务器
 *
 * 服务器 API: POST http://<host>:<port>/api/sensors
 * 请求体: {"deviceid":..., "ph":..., "temperature":...,
 *          "flow":..., "turbidity":..., "conductivity":...}
 */

#ifndef __HTTP_UPLOAD_H__
#define __HTTP_UPLOAD_H__

#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 设备地址 (固定) */
#define DEVICE_ADDR "A5A5A5A5A5A5"

/** 服务器地址 */
#define SERVER_SCHEME "https"
#define SERVER_HOST "esp.ywxk.me"
#define SERVER_PORT 443
#define SERVER_PATH "/api/sensors"
#define SERVER_DEVICE_PATH "/api/devices"

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

#ifdef __cplusplus
}
#endif

#endif /* __HTTP_UPLOAD_H__ */
