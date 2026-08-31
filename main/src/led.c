/**
 * @file led.c
 * @brief 8 路 LED 驱动实现(74HC138 译码 + 轮询刷新)
 */

#include "led.h"

static uint8_t led_pattern;   /**< 期望显示的字节(bit0->Y7, bit7->Y0) */
static uint8_t led_lit[8];    /**< 需要点亮的译码地址列表(元素为 0~7) */
static uint8_t led_count;     /**< 列表中元素个数 */
static uint8_t led_index;     /**< 当前轮询到的列表位置 */

static void led_set_addr(uint8_t addr)
{
    gpio_set_level(LED_A0_PIN, addr & 0x01);
    gpio_set_level(LED_A1_PIN, (addr >> 1) & 0x01);
    gpio_set_level(LED_A2_PIN, (addr >> 2) & 0x01);
}

/**
 * @brief 根据 led_pattern 重建"需要点亮"的地址列表
 *
 * 地址 addr 驱动译码器输出 Y(addr) -> LED(addr),
 * 控制该 LED 的字节位是 addr(bit0 -> Y0 -> LED0)。
 */
static void led_rebuild(void)
{
    led_count = 0;
    led_index = 0;

    for (uint8_t addr = 0; addr < 8; addr++) {
        if (led_pattern & (1U << addr)) {
            led_lit[led_count++] = addr;
        }
    }
}

void led_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << LED_A0_PIN) |
                        (1ULL << LED_A1_PIN) |
                        (1ULL << LED_A2_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);

    led_pattern = 0;
    led_count = 0;
    led_index = 0;
    led_set_addr(0);
}

void set_led(uint8_t pattern)
{
    led_pattern = pattern;
    led_rebuild();
}

void led_refresh(void)
{
    if (led_count == 0) {
        /* 无 LED 需要点亮: 译码器始终有一路输出为低, 无法全部熄灭,
           保持当前地址不变 */
        return;
    }

    uint8_t addr = led_lit[led_index];
    led_index = (led_index + 1) % led_count;

    led_set_addr(addr);
}
