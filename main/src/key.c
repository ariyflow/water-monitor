/**
 * @file key.c
 * @brief 4 路独立按键驱动实现
 */

#include "key.h"

static const gpio_num_t KEY_PINS[KEY_NUM] = {
    GPIO_NUM_48, GPIO_NUM_45, GPIO_NUM_21, GPIO_NUM_47
};

void key_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << KEY_PINS[0]) |
                        (1ULL << KEY_PINS[1]) |
                        (1ULL << KEY_PINS[2]) |
                        (1ULL << KEY_PINS[3]),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
}

bool is_key_down(int key)
{
    if (key < 0 || key >= KEY_NUM) {
        return false;
    }
    return gpio_get_level(KEY_PINS[key]) == KEY_ACTIVE_LEVEL;
}
