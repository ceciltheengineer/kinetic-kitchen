idf_component_register(
    SRCS "main.c"
         "wifi_mgr.c"
         "stepper.c"
         "servo_arm.c"
         "hx711.c"
         "boxes.c"
         "router.c"
         "runlog.c"
         "web_server.c"
    INCLUDE_DIRS "."
    EMBED_TXTFILES "web/index.html"
    PRIV_REQUIRES esp_wifi esp_netif esp_event nvs_flash esp_http_server esp_timer
                  esp_driver_gpio esp_driver_ledc esp_driver_gptimer esp_hal_gpio soc)
