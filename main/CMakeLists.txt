idf_component_register(SRCS "main.c" "voz.c"
                    INCLUDE_DIRS "."
                    REQUIRES board_config placa display tactil cara audio esp_timer
                    EMBED_FILES "voz/hola_soy_trumi.pcm" "voz/hola_sofi.pcm" "voz/hola.pcm"
                                "voz/jiji.pcm" "voz/cosquillas.pcm" "voz/te_quiero.pcm"
                                "voz/otra_vez.pcm")
