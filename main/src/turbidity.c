/**
 * @file turbidity.c
 * @brief TS-300B 浊度传感器模块实现(简易线性浊度指数)
 */

#include "turbidity.h"

#include "adc1_common.h"

static adc_channel_t s_chan;

void turb_init(void)
{
    adc1_common_init();
    adc1_common_add_channel(TURB_PIN, &s_chan);
}

esp_err_t turb_read(float *index)
{
    int mv = 0;

    if (adc1_common_read_mv(s_chan, &mv) != ESP_OK) {
        return ESP_FAIL;
    }

    float v = (float)mv / 1000.0f;
    float idx = 100.0f * (1.0f - v / TURB_REF_V);

    if (idx < 0.0f) {
        idx = 0.0f;
    }
    if (idx > 100.0f) {
        idx = 100.0f;
    }

    *index = idx;
    return ESP_OK;
}
