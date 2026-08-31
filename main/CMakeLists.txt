file(GLOB_RECURSE srcs "main.c" "src/*.c")

idf_component_register(SRCS "${srcs}"
                       PRIV_REQUIRES driver esp_timer esp_adc
                       INCLUDE_DIRS "./include")
