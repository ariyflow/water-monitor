file(GLOB_RECURSE srcs "main.c" "src/*.c")

idf_component_register(SRCS "${srcs}"
                       PRIV_REQUIRES driver esp_timer esp_adc bt nvs_flash
                                    esp_wifi esp_netif esp_event
                                    esp_http_client json mbedtls
                       INCLUDE_DIRS "./include")
