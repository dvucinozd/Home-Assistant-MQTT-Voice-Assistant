idf_component_register(SRCS "mp3_player.c"
                            "wifi_manager.c"
                            "ha_client.c"
                            "tts_player.c"
                            "audio_capture.c"
                            "vad.c"
                            "wwd.c"
                            "mqtt_ha.c"
                    INCLUDE_DIRS "."
                    REQUIRES espressif__esp_websocket_client espressif__mdns json espressif__esp32_p4_function_ev_board bsp_extra chmorgan__esp-libhelix-mp3 espressif__esp-sr mqtt
                    PRIV_REQUIRES esp_wifi driver nvs_flash esp_netif esp_event spiffs fatfs esp_http_client)
