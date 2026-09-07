/**
 * @file ph.c
 * @brief PH 模拟传感器驱动实现(线性电压换算)
 */

#include "ph.h"

#include "adc1_common.h"

static adc_channel_t s_chan;

void ph_init(void)
{
    /* 复用共享 ADC1 层(与 EC/浊度共用同一 oneshot 句柄, 通道独立) */
    adc1_common_init();
    adc1_common_add_channel(PH_PIN, &s_chan);
}

esp_err_t ph_read(float *ph)
{
    int mv = 0;

    if (adc1_common_read_mv(s_chan, &mv) != ESP_OK) {
        return ESP_FAIL;
    }

    float v = (float)mv / 1000.0f;
    *ph = (PH_V_ZERO - v) / PH_SLOPE;
    return ESP_OK;
}
