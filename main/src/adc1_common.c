/**
 * @file adc1_common.c
 * @brief ESP32-S3 ADC1 共享访问层实现
 */

#include "adc1_common.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "soc/soc_caps.h"

#define ADC_SAMPLE_NUM 8    /**< 采样次数(平均滤波) */
#define ADC_FULL_MV    3100 /**< 12dB 衰减满量程(mV, 校准缺失时回退) */

static adc_oneshot_unit_handle_t s_unit = NULL;
static adc_cali_handle_t         s_cali[SOC_ADC_MAX_CHANNEL_NUM] = { NULL };

esp_err_t adc1_common_init(void)
{
    if (s_unit != NULL) {
        return ESP_OK; /* 已创建 */
    }

    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    return adc_oneshot_new_unit(&unit_cfg, &s_unit);
}

esp_err_t adc1_common_add_channel(gpio_num_t pin, adc_channel_t *out_channel)
{
    adc_unit_t    unit;
    adc_channel_t chan;

    if (adc1_common_init() != ESP_OK) {
        return ESP_FAIL;
    }
    if (adc_oneshot_io_to_channel(pin, &unit, &chan) != ESP_OK) {
        return ESP_ERR_NOT_FOUND;
    }
    if (unit != ADC_UNIT_1) {
        return ESP_ERR_INVALID_ARG;
    }

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    if (adc_oneshot_config_channel(s_unit, chan, &chan_cfg) != ESP_OK) {
        return ESP_FAIL;
    }

    /* eFuse 曲线校准(失败则读取时回退到线性换算) */
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1,
        .chan = chan,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali[chan]);

    if (out_channel != NULL) {
        *out_channel = chan;
    }
    return ESP_OK;
}

esp_err_t adc1_common_read_mv(adc_channel_t channel, int *voltage_mv)
{
    int sum = 0;
    int raw = 0;

    for (int i = 0; i < ADC_SAMPLE_NUM; i++) {
        if (adc_oneshot_read(s_unit, channel, &raw) == ESP_OK) {
            sum += raw;
        }
    }
    raw = sum / ADC_SAMPLE_NUM;

    if (s_cali[channel] != NULL) {
        if (adc_cali_raw_to_voltage(s_cali[channel], raw, voltage_mv) != ESP_OK) {
            *voltage_mv = raw * ADC_FULL_MV / 4095;
        }
    } else {
        *voltage_mv = raw * ADC_FULL_MV / 4095;
    }
    return ESP_OK;
}
