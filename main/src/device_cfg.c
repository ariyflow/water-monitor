/**
 * @file device_cfg.c
 * @brief 设备身份配置 NVS 实现
 */

#include <string.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "device_cfg.h"

#define DEV_TAG "device_cfg"

#define DEV_NVS_NAMESPACE "dev_cfg"
#define DEV_KEY_USERNAME  "username"
#define DEV_KEY_SERIAL    "serial"

/** @brief 读取一个字符串键到缓冲, 返回 nvs 状态 */
static esp_err_t dev_get_str(const char *key, char *buf, size_t size)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(DEV_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }

    size_t len = size;
    err = nvs_get_str(handle, key, buf, &len);
    nvs_close(handle);
    return err;
}

/** @brief 写一个字符串键; 值为空则擦除该键 */
static esp_err_t dev_set_str(const char *key, const char *val)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(DEV_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    if (val && val[0]) {
        err = nvs_set_str(handle, key, val);
    } else {
        err = nvs_erase_key(handle, key);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

esp_err_t dev_cfg_save_username(const char *username)
{
    return dev_set_str(DEV_KEY_USERNAME, username);
}

esp_err_t dev_cfg_get_username(char *buf, size_t size)
{
    return dev_get_str(DEV_KEY_USERNAME, buf, size);
}

esp_err_t dev_cfg_save_serial(const char *serial)
{
    return dev_set_str(DEV_KEY_SERIAL, serial);
}

esp_err_t dev_cfg_get_serial(char *buf, size_t size)
{
    return dev_get_str(DEV_KEY_SERIAL, buf, size);
}

bool dev_cfg_has_serial(void)
{
    char buf[DEV_SERIAL_MAX] = {0};
    return (dev_cfg_get_serial(buf, sizeof buf) == ESP_OK && buf[0] != '\0');
}

void dev_cfg_clear(void)
{
    dev_set_str(DEV_KEY_USERNAME, "");
    dev_set_str(DEV_KEY_SERIAL, "");
}
