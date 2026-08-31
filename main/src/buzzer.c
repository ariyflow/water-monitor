/**
 * @file buzzer.c
 * @brief 蜂鸣器驱动实现
 */

#include "buzzer.h"

static void buzzer_set(bool on)
{
    gpio_set_level(BUZZER_PIN, on ? BUZZER_ACTIVE_LEVEL : !BUZZER_ACTIVE_LEVEL);
}

void beep_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << BUZZER_PIN,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);

    buzzer_set(false);
}

void start_beep(void)
{
    buzzer_set(true);
}

void stop_beep(void)
{
    buzzer_set(false);
}
