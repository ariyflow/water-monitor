/**
 * @file http_upload.c
 * @brief HTTP 上报实现 (esp_http_client)
 *
 * 提供两类请求:
 *   1. 传感器数据上报: POST /api/sensors, 携带设备序列号(优先)或预留设备地址。
 *   2. 申请设备序列号: POST /api/devices, 用配网用户名换取序列号。
 * 均为阻塞式, 应放在独立任务中, 避免阻塞其他任务。
 */

#include <stdio.h>
#include <string.h>
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "cJSON.h"
#include "http_upload.h"
#include "device_cfg.h"

#define UPLOAD_TAG "http_upload"

#define HTTP_TIMEOUT_MS 10000

/** @brief HTTP 响应体收集上下文 */
typedef struct {
    char  buf[512];
    size_t len;
} http_body_t;

/** @brief 收集响应体数据事件, 追加到 http_body_t */
static esp_err_t body_event_handler(esp_http_client_event_t *ev)
{
    http_body_t *body = (http_body_t *)ev->user_data;

    if (ev->event_id == HTTP_EVENT_ON_DATA && ev->data && ev->data_len) {
        if (body && body->len + ev->data_len < sizeof body->buf) {
            memcpy(body->buf + body->len, ev->data, ev->data_len);
            body->len += ev->data_len;
            body->buf[body->len] = '\0';
        }
    }
    return ESP_OK;
}

esp_err_t wm_http_upload(float ph, float temperature, float flow,
                         float turbidity, int conductivity)
{
    char body[256];
    char url[128];
    char serial[DEV_SERIAL_MAX] = {0};
    int  len;

    dev_cfg_get_serial(serial, sizeof serial);

    /* 已绑定序列号则上报 serial 字段; 否则回退到固定 deviceid */
    if (serial[0]) {
        len = snprintf(
            body, sizeof body,
            "{\"serial\":\"%s\",\"ph\":%.1f,\"temperature\":%.2f,"
            "\"flow\":%.2f,\"turbidity\":%.0f,\"conductivity\":%d}",
            serial, ph, temperature, flow, turbidity, conductivity);
    } else {
        len = snprintf(
            body, sizeof body,
            "{\"deviceid\":\"%s\",\"ph\":%.1f,\"temperature\":%.2f,"
            "\"flow\":%.2f,\"turbidity\":%.0f,\"conductivity\":%d}",
            DEVICE_ADDR, ph, temperature, flow, turbidity, conductivity);
    }
    if (len <= 0 || len >= (int)sizeof body) {
        ESP_LOGE(UPLOAD_TAG, "json too long (%d)", len);
        return ESP_ERR_INVALID_SIZE;
    }

    snprintf(url, sizeof url, "%s://%s:%d%s", SERVER_SCHEME, SERVER_HOST,
             SERVER_PORT, SERVER_PATH);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(UPLOAD_TAG, "http client init failed");
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body, len);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(UPLOAD_TAG, "POST %s -> %d", url, status);
        if (status == 404) {
            /* 设备/序列号无效: 清除已绑定的序列号, 交给 bind_task 重新申请 */
            dev_cfg_save_serial("");
        }
        if (status < 200 || status >= 300) {
            err = ESP_FAIL;
        }
    } else {
        ESP_LOGW(UPLOAD_TAG, "POST %s failed: %s", url, esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}

esp_err_t wm_http_fetch_serial(const char *username, char *out, size_t out_size)
{
    esp_err_t err = ESP_FAIL;
    char body[128];
    char url[128];
    http_body_t resp = {0};

    if (!username || username[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (!out || out_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    out[0] = '\0';

    snprintf(body, sizeof body, "{\"username\":\"%s\"}", username);
    snprintf(url, sizeof url, "%s://%s:%d%s", SERVER_SCHEME, SERVER_HOST,
             SERVER_PORT, SERVER_DEVICE_PATH);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .event_handler = body_event_handler,
        .user_data = &resp,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(UPLOAD_TAG, "register: http client init failed");
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body, strlen(body));

    esp_err_t perf = esp_http_client_perform(client);
    if (perf == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(UPLOAD_TAG, "register POST %s -> %d", url, status);
        if (status >= 200 && status < 300) {
            cJSON *root = cJSON_Parse(resp.buf);
            if (root) {
                cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
                cJSON *serial = data ?
                    cJSON_GetObjectItemCaseSensitive(data, "serial") : NULL;
                if (cJSON_IsString(serial) && serial->valuestring &&
                    serial->valuestring[0]) {
                    strncpy(out, serial->valuestring, out_size - 1);
                    out[out_size - 1] = '\0';
                    err = ESP_OK;
                } else {
                    ESP_LOGW(UPLOAD_TAG, "register: no serial in resp: %s",
                             resp.buf);
                }
                cJSON_Delete(root);
            } else {
                ESP_LOGW(UPLOAD_TAG, "register: bad json: %s", resp.buf);
            }
        } else {
            ESP_LOGW(UPLOAD_TAG, "register: http status %d: %s", status,
                     resp.buf);
        }
    } else {
        ESP_LOGW(UPLOAD_TAG, "register POST %s failed: %s", url,
                 esp_err_to_name(perf));
    }

    esp_http_client_cleanup(client);
    return err;
}

esp_err_t wm_http_fetch_thresholds(const char *serial, alarm_params_t *out)
{
    esp_err_t err = ESP_FAIL;
    char url[160];
    http_body_t resp = {0};

    if (!serial || serial[0] == '\0' || !out) {
        return ESP_ERR_INVALID_ARG;
    }

    snprintf(url, sizeof url, "%s://%s:%d%s?serial=%s", SERVER_SCHEME,
             SERVER_HOST, SERVER_PORT, SERVER_SETTINGS_PATH, serial);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .event_handler = body_event_handler,
        .user_data = &resp,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(UPLOAD_TAG, "settings: http client init failed");
        return ESP_FAIL;
    }

    esp_err_t perf = esp_http_client_perform(client);
    if (perf == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(UPLOAD_TAG, "settings GET %s -> %d", url, status);
        if (status >= 200 && status < 300) {
            cJSON *root = cJSON_Parse(resp.buf);
            if (root) {
                cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
                float val;
                if (!data) {
                    ESP_LOGW(UPLOAD_TAG, "settings: no data in resp: %s",
                             resp.buf);
                } else {
                    cJSON *it = cJSON_GetObjectItemCaseSensitive(data, "temp_low_c");
                    val = cJSON_IsNumber(it) ? (float)it->valuedouble : out->temp_low_c;
                    out->temp_low_c = val;
                    it = cJSON_GetObjectItemCaseSensitive(data, "temp_high_c");
                    val = cJSON_IsNumber(it) ? (float)it->valuedouble : out->temp_high_c;
                    out->temp_high_c = val;
                    it = cJSON_GetObjectItemCaseSensitive(data, "flow_high_lpm");
                    val = cJSON_IsNumber(it) ? (float)it->valuedouble : out->flow_high_lpm;
                    out->flow_high_lpm = val;
                    it = cJSON_GetObjectItemCaseSensitive(data, "ec_high_us_cm");
                    val = cJSON_IsNumber(it) ? (float)it->valuedouble : out->ec_high_us_cm;
                    out->ec_high_us_cm = val;
                    it = cJSON_GetObjectItemCaseSensitive(data, "turb_high_ntu");
                    val = cJSON_IsNumber(it) ? (float)it->valuedouble : out->turb_high_ntu;
                    out->turb_high_ntu = val;
                    err = ESP_OK;
                }
                cJSON_Delete(root);
            } else {
                ESP_LOGW(UPLOAD_TAG, "settings: bad json: %s", resp.buf);
            }
        } else {
            ESP_LOGW(UPLOAD_TAG, "settings: http status %d: %s", status,
                     resp.buf);
        }
    } else {
        ESP_LOGW(UPLOAD_TAG, "settings GET %s failed: %s", url,
                 esp_err_to_name(perf));
    }

    esp_http_client_cleanup(client);
    return err;
}

esp_err_t wm_http_report_alarm(const char *serial, const char *type, float value,
                               float threshold, float ph, float temperature,
                               float flow, float turbidity, int conductivity,
                               const char *message)
{
    char body[256];
    char url[128];
    int  len;

    if (!serial || !serial[0]) {
        ESP_LOGE(UPLOAD_TAG, "alarm: no serial");
        return ESP_ERR_INVALID_ARG;
    }

    len = snprintf(
        body, sizeof body,
        "{\"serial\":\"%s\",\"active\":true,\"type\":\"%s\",\"value\":%.2f,"
        "\"threshold\":%.2f,\"ph\":%.1f,\"temperature\":%.2f,\"flow\":%.2f,"
        "\"turbidity\":%.0f,\"conductivity\":%d,\"message\":\"%s\"}",
        serial, type ? type : "", (double)value, (double)threshold, (double)ph,
        (double)temperature, (double)flow, (double)turbidity, conductivity,
        message ? message : "");
    if (len <= 0 || len >= (int)sizeof body) {
        ESP_LOGE(UPLOAD_TAG, "alarm json too long (%d)", len);
        return ESP_ERR_INVALID_SIZE;
    }

    snprintf(url, sizeof url, "%s://%s:%d%s", SERVER_SCHEME, SERVER_HOST,
             SERVER_PORT, SERVER_ALARM_PATH);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(UPLOAD_TAG, "alarm: http client init failed");
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body, len);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(UPLOAD_TAG, "alarm POST %s -> %d", url, status);
        if (status < 200 || status >= 300) {
            err = ESP_FAIL;
        }
    } else {
        ESP_LOGW(UPLOAD_TAG, "alarm POST %s failed: %s", url,
                 esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}
