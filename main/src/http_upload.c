/**
 * @file http_upload.c
 * @brief HTTP POST 上报实现 (esp_http_client)
 *
 * 通过 esp_http_client 向服务器 Post 一个 JSON 请求体, 服务器收到后写入数据库。
 * 上报为阻塞式, 应放在独立任务中, 避免阻塞传感器采样。
 */

#include <stdio.h>
#include <string.h>
#include "esp_http_client.h"
#include "esp_log.h"
#include "http_upload.h"

#define UPLOAD_TAG "http_upload"

#define HTTP_TIMEOUT_MS 3000

esp_err_t wm_http_upload(float ph, float temperature, float flow,
                         float turbidity, int conductivity)
{
    char body[256];
    char url[128];

    int len = snprintf(
        body, sizeof body,
        "{\"deviceid\":\"%s\",\"ph\":%.1f,\"temperature\":%.2f,"
        "\"flow\":%.2f,\"turbidity\":%.0f,\"conductivity\":%d}",
        DEVICE_ADDR, ph, temperature, flow, turbidity, conductivity);
    if (len <= 0 || len >= (int)sizeof body) {
        ESP_LOGE(UPLOAD_TAG, "json too long (%d)", len);
        return ESP_ERR_INVALID_SIZE;
    }

    snprintf(url, sizeof url, "http://%s:%d%s", SERVER_HOST, SERVER_PORT,
             SERVER_PATH);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = HTTP_TIMEOUT_MS,
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
        if (status < 200 || status >= 300) {
            err = ESP_FAIL;
        }
    } else {
        ESP_LOGW(UPLOAD_TAG, "POST %s failed: %s", url, esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}
