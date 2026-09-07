/**
 * @file wifi_sta.h
 * @brief WiFi STA 驱动: 接收 SSID/密码并连接, 凭据持久化, 开机自动重连
 *
 * 功能:
 *   - 初始化 WiFi 站模式(STA), 事件驱动连接(连接过程为异步, 不占用调用方任务栈)
 *   - 凭据保存到 NVS, 下次开机自动重连
 *   - 断开后自动退避重试若干次
 *
 * 使用:
 *   1. wifi_sta_init();
 *   2. wifi_sta_connect(ssid, password);
 *   3. wifi_sta_is_connected();  // 查询是否已获取 IP
 */

#ifndef __WIFI_STA_H__
#define __WIFI_STA_H__

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 凭据长度上限 */
#define WIFI_MAX_SSID_LEN 32
#define WIFI_MAX_PASS_LEN 64

/**
 * @brief 初始化 WiFi STA(须在其他外设初始化后调用)
 * @note  若 NVS 中已有凭据, 会自动尝试连接
 * @return ESP_OK 成功; 其他为失败
 */
esp_err_t wifi_sta_init(void);

/**
 * @brief 使用指定 SSID/密码连接 WiFi, 并保存到 NVS
 * @param ssid     WiFi 名称(非空, 长度<=32)
 * @param password 密码(可为空字符串, 长度<=64)
 * @return ESP_OK 配置已下发; 参数非法返回 ESP_ERR_INVALID_ARG
 */
esp_err_t wifi_sta_connect(const char *ssid, const char *password);

/**
 * @brief 查询 WiFi 是否已连接(已获取 IP)
 */
bool wifi_sta_is_connected(void);

/**
 * @brief 清除 WiFi 配置并断开
 * @note  擦除 NVS 中保存的 SSID/密码, 停止 WiFi 以便设备回到未配网状态
 */
void wifi_sta_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* __WIFI_STA_H__ */
