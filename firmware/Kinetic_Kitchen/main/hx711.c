#include "config.h"
#include "hx711.h"

#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "hx711";

#if KK_SIM_MODE
#define SIM_ZERO_COUNTS 123456   // a made-up zero offset, like a real cell has
static float s_sim_g[KK_NUM_CELLS];

void hx711_init(void)
{
    ESP_LOGW(TAG, "SIMULATION: load cells are faked in software");
}

bool hx711_read_raw(int cell, int32_t *raw)
{
    if (cell < 0 || cell >= KK_NUM_CELLS) {
        return false;
    }
    // +-0.3 g of noise at the default scale
    int32_t noise = (rand() % 601) - 300;
    *raw = SIM_ZERO_COUNTS + (int32_t)(s_sim_g[cell] * KK_HX_DEFAULT_SCALE) + noise;
    return true;
}

void hx711_sim_set_grams(int cell, float grams)
{
    if (cell >= 0 && cell < KK_NUM_CELLS) {
        s_sim_g[cell] = grams;
        ESP_LOGI(TAG, "SIM cell %d = %.1f g", cell, grams);
    }
}

float hx711_sim_get_grams(int cell)
{
    return (cell >= 0 && cell < KK_NUM_CELLS) ? s_sim_g[cell] : 0.0f;
}

#else  // real hardware

typedef struct {
    int dout;
    int sck;
} cell_pins_t;

static const cell_pins_t s_cells[KK_NUM_CELLS] = {
    { KK_PIN_HX_BOX2_DOUT, KK_PIN_HX_BOX2_SCK },
    { KK_PIN_HX_BOX4_DOUT, KK_PIN_HX_BOX4_SCK },
};

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

void hx711_init(void)
{
    for (int i = 0; i < KK_NUM_CELLS; i++) {
        gpio_config_t in = {
            .pin_bit_mask = 1ULL << s_cells[i].dout,
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&in));
        gpio_config_t out = {
            .pin_bit_mask = 1ULL << s_cells[i].sck,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&out));
        gpio_set_level(s_cells[i].sck, 0);   // SCK low = powered up
    }
    ESP_LOGI(TAG, "%d load cells", KK_NUM_CELLS);
}

bool hx711_read_raw(int cell, int32_t *raw)
{
    if (cell < 0 || cell >= KK_NUM_CELLS) {
        return false;
    }
    const cell_pins_t *c = &s_cells[cell];

    // Data is ready when DOUT goes low.
    int64_t t0 = esp_timer_get_time();
    while (gpio_get_level(c->dout)) {
        if (esp_timer_get_time() - t0 > 200 * 1000) {
            return false;   // not wired, or the chip is powered down
        }
        vTaskDelay(1);
    }

    // The clock burst must not be interrupted: SCK high for > 60 us powers
    // the chip down and ruins the reading.
    uint32_t v = 0;
    portENTER_CRITICAL(&s_mux);
    for (int i = 0; i < 24; i++) {
        gpio_set_level(c->sck, 1);
        esp_rom_delay_us(1);
        v = (v << 1) | (uint32_t)gpio_get_level(c->dout);
        gpio_set_level(c->sck, 0);
        esp_rom_delay_us(1);
    }
    // One extra pulse selects channel A, gain 128 for the next conversion.
    gpio_set_level(c->sck, 1);
    esp_rom_delay_us(1);
    gpio_set_level(c->sck, 0);
    esp_rom_delay_us(1);
    portEXIT_CRITICAL(&s_mux);

    if (v & 0x800000) {
        v |= 0xFF000000;   // sign-extend 24 -> 32 bit
    }
    *raw = (int32_t)v;
    return true;
}

#endif
