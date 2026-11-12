// Kinetic Kitchen - ESP32 firmware entry point.
//
// Sorting conveyor for a 3D print farm: three start positions, five boxes,
// a reversible belt, three servo arms and two load cells. The ESP32 serves
// the dashboard page and runs the routes. See README.md for wiring and build.

#include "config.h"
#include "stepper.h"
#include "servo_arm.h"
#include "boxes.h"
#include "router.h"
#include "runlog.h"
#include "wifi_mgr.h"
#include "web_server.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "esp_log.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "Kinetic Kitchen firmware %s%s", KK_FW_VERSION, KK_SIM_MODE ? " (SIMULATION)" : "");

    esp_err_t r = nvs_flash_init();
    if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        r = nvs_flash_init();
    }
    ESP_ERROR_CHECK(r);

    gpio_config_t led = {
        .pin_bit_mask = 1ULL << KK_PIN_LED,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&led);

    runlog_init();
    stepper_init();      // on core 0: the step ISR lives here
    servo_arm_init();
    boxes_init();        // sampling task on core 1
    router_init();       // sequencer task on core 1

    wifi_mgr_start();
    web_server_start();

    wifi_info_t wi;
    wifi_mgr_get_info(&wi);
    ESP_LOGI(TAG, "ready. Open http://%s", wi.ip);

    // Heartbeat: slow blink when idle, fast while a route runs.
    bool on = false;
    for (;;) {
        on = !on;
        gpio_set_level(KK_PIN_LED, on);
        vTaskDelay(pdMS_TO_TICKS(router_busy() ? 100 : 800));
    }
}
