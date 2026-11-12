idf_component_register(SRCS "audio.c"
                       INCLUDE_DIRS "include"
                       REQUIRES placa board_config esp_driver_i2s esp_driver_i2c)
