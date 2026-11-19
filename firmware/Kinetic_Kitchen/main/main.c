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
#include "tmc_uart.h"
#include "driver_check.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "esp_log.h"

static const char *TAG = "main";

#if KK_MOTOR_CONNECTED && KK_MOTOR_TEST && !KK_DRIVER_CHECK
static void wait_move(void)
{
    while (stepper_is_moving()) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Bench test: stepper only. Watch the serial monitor and the belt.
static void motor_test(void)
{
    ESP_LOGW(TAG, "MOTOR TEST MODE: STEP=%d DIR=%d EN=%d", KK_PIN_STEP, KK_PIN_DIR, KK_PIN_EN);
    stepper_init();
#if KK_TMC_UART_ENABLED
    tmc_configure();
#endif
    for (int cycle = 1;; cycle++) {
        ESP_LOGI(TAG, "cycle %d: holding 3 s. The shaft should be stiff now", cycle);
        stepper_enable(true);
        vTaskDelay(pdMS_TO_TICKS(3000));
        ESP_LOGI(TAG, "cycle %d: forward %.0f mm", cycle, KK_TEST_MOVE_MM);
        gpio_set_level(KK_PIN_LED, 1);
        stepper_move_mm(KK_TEST_MOVE_MM, +1);
        wait_move();
        vTaskDelay(pdMS_TO_TICKS(1000));

        ESP_LOGI(TAG, "cycle %d: reverse %.0f mm", cycle, KK_TEST_MOVE_MM);
        stepper_move_mm(KK_TEST_MOVE_MM, -1);
        wait_move();

        ESP_LOGI(TAG, "cycle %d: done, moved %.1f mm. Motor off for 3 s. The shaft should turn freely", cycle, stepper_mm_moved());
        stepper_enable(false);
        gpio_set_level(KK_PIN_LED, 0);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
#endif

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

#if !KK_MOTOR_CONNECTED
    ESP_LOGW(TAG, "KK_MOTOR_CONNECTED is 0: load cells and servo arms only, no belt");
#elif KK_DRIVER_CHECK
    driver_check_run();   // never returns
#elif KK_MOTOR_TEST
    motor_test();         // never returns
#endif

    runlog_init();
    // Wi-Fi first: its start-up current peak must not coincide with the servos
    // jumping to rest, or a weak 5 V supply browns out and the board resets.
    wifi_mgr_start();
    stepper_init();      // on core 0: the step ISR lives here
#if KK_TMC_UART_ENABLED && KK_MOTOR_CONNECTED
    tmc_configure();     // motor current over UART; falls back to the knob
#endif
    servo_arm_init();
    boxes_init();        // sampling task on core 1
    router_init();       // sequencer task on core 1

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
