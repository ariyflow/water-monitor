/**
 * @file ble.h
 * @brief BLE 透传接口 (手机 -> 单片机单向写入)
 *
 * 功能:
 *   - 以 BLE Peripheral (GATT Server) 角色广播, 手机连接后可发现服务
 *   - 自定义服务 (UUID 0xFFE0) 内含一个写特征 (UUID 0xFFE1),
 *     手机向该特征写入的任意字节会通过 wm_ble_set_data_cb 注册的回调交给应用层
 *   - 无配对/无加密, 断连后自动恢复广播
 *
 * 注意: 数据回调运行在 NimBLE 主机任务上下文中, 必须快速返回, 不得阻塞
 */

#ifndef __BLE_H__
#define __BLE_H__

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** BLE 广播设备名 */
#define WM_BLE_DEVICE_NAME "WM-BLE"

/** 单次写请求接收缓冲上限 (字节) */
#define WM_BLE_RX_BUF_SIZE 512

/**
 * @brief BLE 数据接收回调
 * @param data 收到的数据(仅在回调内有效)
 * @param len  数据长度
 */
typedef void (*wm_ble_data_cb_t)(const uint8_t *data, size_t len);

/**
 * @brief 初始化 BLE 从机(不启动广播)
 * @note  应在传感器/外设初始化之后调用; 内部会初始化 NVS 与 NimBLE 栈。
 *        初始化后处于待机状态, 需调用 wm_ble_start() 才开始广播。
 * @return ESP_OK 成功; 其他为失败
 */
esp_err_t wm_ble_init(void);

/**
 * @brief 启动 BLE 广播(开启蓝牙)
 * @note  若已连接则停止广播; 断连后自动恢复广播
 */
void wm_ble_start(void);

/**
 * @brief 停止 BLE 广播(关闭蓝牙)
 * @note  若当前已连接, 会同时断开连接
 */
void wm_ble_stop(void);

/**
 * @brief 注册数据接收回调
 * @param cb 回调函数, 传 NULL 可取消注册
 */
void wm_ble_set_data_cb(wm_ble_data_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif /* __BLE_H__ */
