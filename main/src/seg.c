/**
 * @file seg.c
 * @brief 4 位 7 段共阴数码管驱动实现(动态扫描)
 */

#include "seg.h"

/** 段码译码表: 索引 0~9 -> 0~9, 10~15 -> A~F */
static const uint8_t SEG_CODE[16] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, /* 0 1 2 3 4 5 6 7 */
    0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71  /* 8 9 A b C d E F */
};

/** 段选引脚, 顺序为 a b c d e f g dp */
static const gpio_num_t SEG_PINS[8] = {
    GPIO_NUM_18, GPIO_NUM_16, GPIO_NUM_9, GPIO_NUM_3,
    GPIO_NUM_8,  GPIO_NUM_17, GPIO_NUM_10, GPIO_NUM_46
};

static uint8_t seg_idx[4];     /**< 4 位译码表索引 (0~15) */
static bool     seg_point[4];  /**< 4 位小数点标志 */

/** @brief 写出段码到段选引脚 */
static void seg_write(uint8_t code)
{
    for (int i = 0; i < 8; i++) {
        gpio_set_level(SEG_PINS[i], (code >> i) & 1);
    }
}

/** @brief 选中某一位位选 (digit: 0~3) */
static void seg_select(int digit)
{
    gpio_set_level(SEG_DIG_A0_PIN, digit & 1);
    gpio_set_level(SEG_DIG_A1_PIN, (digit >> 1) & 1);
}

void seg_init(void)
{
    gpio_config_t dig_cfg = {
        .pin_bit_mask = (1ULL << SEG_DIG_A0_PIN) | (1ULL << SEG_DIG_A1_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&dig_cfg);

    for (int i = 0; i < 8; i++) {
        gpio_config_t seg_cfg = {
            .pin_bit_mask = 1ULL << SEG_PINS[i],
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&seg_cfg);
    }

    for (int i = 0; i < 4; i++) {
        seg_idx[i] = 0;
        seg_point[i] = false;
    }
    seg_select(0);
    seg_write(0);
}

void set_seg(int s0, int s1, int s2, int s3)
{
    int in[4] = { s0, s1, s2, s3 };

    for (int i = 0; i < 4; i++) {
        if (in[i] < 0)   in[i] = 0;
        if (in[i] > 15)  in[i] = 15;
        seg_idx[i] = (uint8_t)in[i];
    }
}

void set_num(int num)
{
    if (num < 0)   num = 0;
    if (num > 9999) num = 9999;

    set_seg(num / 1000 % 10, num / 100 % 10, num / 10 % 10, num % 10);

    for (int i = 0; i < 4; i++) {
        seg_point[i] = false;
    }
}

void set_point(int digit)
{
    for (int i = 0; i < 4; i++) {
        seg_point[i] = false;
    }
    if (digit >= 0 && digit <= 3) {
        seg_point[digit] = true;
    }
}

void seg_refresh(void)
{
    static int seg_digit; /* 当前扫描到的位 */

    seg_write(0); /* 先熄灭段码, 防止换位残影 */
    seg_select(seg_digit);
    seg_write(SEG_CODE[seg_idx[seg_digit]] | (seg_point[seg_digit] ? 0x80 : 0x00));

    seg_digit = (seg_digit + 1) % 4;
}
