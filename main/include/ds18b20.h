/**
 * @file ds18b20.h
 * @brief DS18B20 单总线温度传感器驱动(位时序法)
 *
 * 硬件连接:
 *   - 数据线: GPIO5
 *   - 上拉: 建议数据线接 4.7k 上拉到 3.3V; 驱动内部也已使能弱上拉辅助
 *
 * 使用:
 *   1. ds18b20_init(GPIO_NUM_5);
 *   2. ds18b20_start_conversion();   // 启动温度转换
 *   3. 等待约 750ms (12bit 分辨率)
 *   4. ds18b20_read_temp(&temp);     // 读取温度
 */

#ifndef __DS18B20_H__
#define __DS18B20_H__

#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DS18B20_PIN GPIO_NUM_5

/** @brief 初始化数据线引脚(开漏输出 + 内部上拉) */
void ds18b20_init(gpio_num_t pin);

/**
 * @brief 复位总线并发出转换温度命令(Convert T)
 * @return ESP_OK 成功; ESP_ERR_NOT_FOUND 总线上无 DS18B20
 * @note  调用后需等待约 750ms 再读取, 期间可执行其他工作
 */
esp_err_t ds18b20_start_conversion(void);

/**
 * @brief 读取一次温度
 * @param temp 输出的温度值(℃)
 * @return ESP_OK 成功; ESP_ERR_NOT_FOUND 无设备; ESP_ERR_INVALID_CRC 数据校验失败
 * @note  必须先调用 ds18b20_start_conversion() 并等待转换完成
 */
esp_err_t ds18b20_read_temp(float *temp);

#ifdef __cplusplus
}
#endif

#endif /* __DS18B20_H__ */
