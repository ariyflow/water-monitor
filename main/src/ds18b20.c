/**
 * @file ds18b20.c
 * @brief DS18B20 单总线温度传感器驱动实现(位时序法)
 *
 * 采用 GPIO 开漏输出实现 1-Wire 总线时序, 高电平通过上拉电阻释放。
 * 时间参数参考 DS18B20 数据手册 (12bit 分辨率)。
 */

#include "ds18b20.h"

#include "esp_rom_sys.h"

static gpio_num_t ow_pin;

/** @brief 阻塞延时 us, 用于时序控制 */
static inline void ow_delay_us(uint32_t us)
{
    esp_rom_delay_us(us);
}

/** @brief 总线拉低 */
static inline void ow_low(void)
{
    gpio_set_level(ow_pin, 0);
}

/** @brief 总线释放(置高, 开漏下由上拉电阻拉高) */
static inline void ow_high(void)
{
    gpio_set_level(ow_pin, 1);
}

/**
 * @brief 复位总线并检测器件存在
 * @return true 检测到存在脉冲; false 无器件
 */
static bool ow_reset(void)
{
    bool present;

    ow_low();
    ow_delay_us(500);
    ow_high();
    ow_delay_us(60);
    present = (gpio_get_level(ow_pin) == 0);
    ow_delay_us(400);

    return present;
}

/** @brief 写 1 位 (bit=0 写 0, bit=1 写 1) */
static void ow_write_bit(bool bit)
{
    ow_low();
    if (bit) {
        ow_delay_us(8);
        ow_high();
        ow_delay_us(62);
    } else {
        ow_delay_us(65);
        ow_high();
        ow_delay_us(5);
    }
}

/** @brief 读 1 位 (返回 0 或 1) */
static int ow_read_bit(void)
{
    int val;

    ow_low();
    ow_delay_us(6);
    ow_high();
    ow_delay_us(9);
    val = gpio_get_level(ow_pin);
    ow_delay_us(50);

    return val;
}

/** @brief 写 1 字节 (LSB first) */
static void ow_write_byte(uint8_t byte)
{
    for (int i = 0; i < 8; i++) {
        ow_write_bit((byte >> i) & 0x01);
    }
}

/** @brief 读 1 字节 (LSB first) */
static uint8_t ow_read_byte(void)
{
    uint8_t byte = 0;

    for (int i = 0; i < 8; i++) {
        if (ow_read_bit()) {
            byte |= (1U << i);
        }
    }
    return byte;
}

/**
 * @brief 计算 CRC-8 (多项式 x^8 + x^5 + x^4 + 1, 初值 0)
 */
static uint8_t ow_crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0;

    for (size_t i = 0; i < len; i++) {
        uint8_t byte = data[i];
        for (int b = 0; b < 8; b++) {
            uint8_t mix = (crc ^ byte) & 0x01;
            crc >>= 1;
            if (mix) {
                crc ^= 0x8C;
            }
            byte >>= 1;
        }
    }
    return crc;
}

void ds18b20_init(gpio_num_t pin)
{
    ow_pin = pin;

    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << ow_pin,
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);

    ow_high();
}

esp_err_t ds18b20_start_conversion(void)
{
    if (!ow_reset()) {
        return ESP_ERR_NOT_FOUND;
    }
    ow_write_byte(0xCC); /* Skip ROM */
    ow_write_byte(0x44); /* Convert T */

    return ESP_OK;
}

static esp_err_t ds18b20_read_raw(int16_t *raw)
{
    uint8_t data[9];

    if (!ow_reset()) {
        return ESP_ERR_NOT_FOUND;
    }
    ow_write_byte(0xCC); /* Skip ROM */
    ow_write_byte(0xBE); /* Read Scratchpad */

    for (int i = 0; i < 9; i++) {
        data[i] = ow_read_byte();
    }
    if (ow_crc8(data, 8) != data[8]) {
        return ESP_ERR_INVALID_CRC;
    }

    *raw = (int16_t)((data[1] << 8) | data[0]);
    return ESP_OK;
}

esp_err_t ds18b20_read_temp(float *temp)
{
    int16_t raw;
    esp_err_t err = ds18b20_read_raw(&raw);

    if (err != ESP_OK) {
        return err;
    }

    *temp = (float)raw * 0.0625f;
    return ESP_OK;
}
