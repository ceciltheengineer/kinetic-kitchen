#include "stepper.h"
#include "config.h"

#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "hal/gpio_ll.h"
#include "soc/gpio_struct.h"
#include "esp_attr.h"
#include "esp_log.h"

static const char *TAG = "stepper";

#define TIMER_HZ 10000000u   // 0.1 us ticks
#define RAMP_MAX 4096        // max steps in the acceleration ramp

static gptimer_handle_t s_timer;
static bool     s_timer_started;
static bool     s_enabled;
static float    s_steps_per_mm;
static uint32_t s_ramp[RAMP_MAX];   // step period (ticks) for ramp step n; n = 0 is slowest
static uint32_t s_ramp_len;
static uint32_t s_cruise;           // step period at full speed

static volatile bool     s_running;
static volatile bool     s_level;      // current STEP pin level
static volatile bool     s_stop_req;
static volatile uint32_t s_done;       // steps completed in this move
static volatile uint32_t s_remaining;  // steps left in this move
static volatile int      s_dir = 1;

// Runs twice per step: rising edge, then falling edge.
static bool IRAM_ATTR on_alarm(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *ctx)
{
    if (!s_running) {
        return false;
    }
    if (!s_level) {
        gpio_ll_set_level(&GPIO, KK_PIN_STEP, 1);
        s_level = true;
        return false;
    }
    gpio_ll_set_level(&GPIO, KK_PIN_STEP, 0);
    s_level = false;
    s_done++;
    s_remaining--;

    if (s_stop_req) {
        // Shorten the move so it ends with a normal deceleration.
        s_stop_req = false;
        uint32_t d = (s_done < s_ramp_len) ? s_done : s_ramp_len;
        if (s_remaining > d) {
            s_remaining = d;
        }
    }
    if (s_remaining == 0) {
        s_running = false;
        return false;
    }

    // Accelerate on the way out, decelerate symmetrically at the end.
    uint32_t idx = (s_remaining < s_done) ? s_remaining : s_done;
    if (idx >= s_ramp_len) {
        idx = s_ramp_len - 1;
    }
    uint32_t period = s_ramp[idx];
    if (period < s_cruise) {
        period = s_cruise;
    }
    gptimer_alarm_config_t a = {
        .alarm_count = period / 2,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true,
    };
    gptimer_set_alarm_action(timer, &a);
    return false;
}

static void build_ramp(float speed_mm_s, float accel_mm_s2)
{
    float v = speed_mm_s * s_steps_per_mm;   // steps/s
    float a = accel_mm_s2 * s_steps_per_mm;  // steps/s^2
    s_cruise = (uint32_t)((float)TIMER_HZ / v);
    // Classic constant-acceleration ramp: c_n = c0 * (sqrt(n+1) - sqrt(n))
    float c0 = (float)TIMER_HZ * sqrtf(2.0f / a);
    uint32_t n = 0;
    for (; n < RAMP_MAX; n++) {
        float c = c0 * (sqrtf((float)n + 1.0f) - sqrtf((float)n));
        if (c <= (float)s_cruise) {
            break;
        }
        s_ramp[n] = (uint32_t)c;
    }
    if (n == 0) {
        s_ramp[0] = s_cruise;
        n = 1;
    }
    s_ramp_len = n;
    ESP_LOGI(TAG, "%.1f steps/mm, cruise %.0f steps/s, ramp %lu steps (%.1f mm)",
             s_steps_per_mm, v, (unsigned long)n, n / s_steps_per_mm);
}

void stepper_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << KK_PIN_STEP) | (1ULL << KK_PIN_DIR) | (1ULL << KK_PIN_EN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    gpio_set_level(KK_PIN_STEP, 0);
    gpio_set_level(KK_PIN_DIR, KK_DIR_FORWARD_LEVEL);
    stepper_enable(false);

    s_steps_per_mm = (float)KK_MOTOR_STEPS_PER_REV * (float)KK_MICROSTEPS * KK_PULLEY_RATIO
                     / ((float)M_PI * KK_ROLLER_DIAMETER_MM);
#if KK_MOTOR_TEST || KK_DRIVER_CHECK
    build_ramp(KK_TEST_SPEED_MM_S, KK_TEST_ACCEL_MM_S2);
#else
    build_ramp(KK_BELT_SPEED_MM_S, KK_BELT_ACCEL_MM_S2);
#endif

    gptimer_config_t tc = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = TIMER_HZ,
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&tc, &s_timer));
    gptimer_event_callbacks_t cbs = { .on_alarm = on_alarm };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(s_timer, &cbs, NULL));
    ESP_ERROR_CHECK(gptimer_enable(s_timer));
}

void stepper_enable(bool on)
{
    s_enabled = on;
    gpio_set_level(KK_PIN_EN, on ? 0 : 1);   // active low
}

bool stepper_enabled(void)
{
    return s_enabled;
}

static void housekeeping(void)
{
    if (!s_running && s_timer_started) {
        gptimer_stop(s_timer);
        s_timer_started = false;
        gpio_set_level(KK_PIN_STEP, 0);
        s_level = false;
    }
}

bool stepper_move_mm(float mm, int dir)
{
    housekeeping();
    if (s_running || mm <= 0.0f) {
        return false;
    }
    uint32_t steps = (uint32_t)(mm * s_steps_per_mm + 0.5f);
    if (steps == 0) {
        return false;
    }
    s_dir = (dir >= 0) ? 1 : -1;
    gpio_set_level(KK_PIN_DIR, (s_dir > 0) ? KK_DIR_FORWARD_LEVEL : !KK_DIR_FORWARD_LEVEL);
    if (!s_enabled) {
        stepper_enable(true);
    }
    vTaskDelay(pdMS_TO_TICKS(2));   // the driver wants a moment after EN / DIR changes

    s_done = 0;
    s_remaining = steps;
    s_level = false;
    s_stop_req = false;
    gptimer_alarm_config_t a = {
        .alarm_count = s_ramp[0] / 2,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(s_timer, &a));
    ESP_ERROR_CHECK(gptimer_set_raw_count(s_timer, 0));
    s_running = true;
    ESP_ERROR_CHECK(gptimer_start(s_timer));
    s_timer_started = true;
    ESP_LOGI(TAG, "move %.0f mm dir %d (%lu steps)", mm, s_dir, (unsigned long)steps);
    return true;
}

void stepper_stop(void)
{
    if (s_running) {
        s_stop_req = true;
    }
}

void stepper_abort(void)
{
    s_running = false;
    housekeeping();
}

bool stepper_is_moving(void)
{
    housekeeping();
    return s_running;
}

float stepper_mm_moved(void)
{
    return (float)s_done / s_steps_per_mm;
}

int stepper_dir(void)
{
    return s_dir;
}

float stepper_steps_per_mm(void)
{
    return s_steps_per_mm;
}
