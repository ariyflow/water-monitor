/**
 * @file device_cfg.h
 * @brief 设备身份配置(非易失存储 NVS)
 *
 * 用于持久化设备在配网时从服务器获取的身份信息, 与 WiFi 凭据一样保存在 NVS,
 * 断电不丢失:
 *   - username : 配网时手机提供的用户名(用于到服务器获取序列号)
 *   - serial   : 服务器下发、用于上报传感器数据的设备序列号
 *
 * 使用:
 *   1. 配网后: dev_cfg_save_username(user);  dev_cfg_save_serial(serial);
 *   2. 开机:   dev_cfg_get_username(...);     dev_cfg_get_serial(...);
 *   3. dev_cfg_has_serial() 判断是否已绑定序列号。
 */

#ifndef __DEVICE_CFG_H__
#define __DEVICE_CFG_H__

#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 用户名字符串长度上限(含空字符) */
#define DEV_USERNAME_MAX 64

/** 序列号字符串长度上限(含空字符; 序列号通常为 12 位十六进制) */
#define DEV_SERIAL_MAX 32

/**
 * @brief 保存用户名到 NVS
 * @param username 用户名; 传空指针或空串则擦除该键
 * @return ESP_OK 成功
 */
esp_err_t dev_cfg_save_username(const char *username);

/**
 * @brief 从 NVS 读取用户名
 * @param buf  输出缓冲
 * @param size 缓冲大小
 * @return ESP_OK 成功; 其他为失败(未存储返回 ESP_ERR_NVS_NOT_FOUND)
 */
esp_err_t dev_cfg_get_username(char *buf, size_t size);

/**
 * @brief 保存序列号到 NVS
 * @param serial 设备序列号; 传空指针或空串则擦除该键
 * @return ESP_OK 成功
 */
esp_err_t dev_cfg_save_serial(const char *serial);

/**
 * @brief 从 NVS 读取序列号
 * @param buf  输出缓冲
 * @param size 缓冲大小
 * @return ESP_OK 成功; 其他为失败
 */
esp_err_t dev_cfg_get_serial(char *buf, size_t size);

/**
 * @brief 判断是否已绑定设备序列号
 * @return true = 已保存且非空序列号; false = 未绑定
 */
bool dev_cfg_has_serial(void);

#ifdef __cplusplus
}
#endif

#endif /* __DEVICE_CFG_H__ */
