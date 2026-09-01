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

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 设备地址 (固定) */
#define DEVICE_ADDR "A5A5A5A5A5A5"

/** 服务器地址 */
#define SERVER_HOST "10.11.172.5"
#define SERVER_PORT 15382
#define SERVER_PATH "/api/sensors"

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

#ifdef __cplusplus
}
#endif

#endif /* __HTTP_UPLOAD_H__ */
