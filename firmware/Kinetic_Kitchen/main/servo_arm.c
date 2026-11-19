#include "servo_arm.h"
#include "config.h"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "servo";

#define SERVO_FREQ_HZ 50
#define SERVO_PERIOD_US 20000
// 14 bits works on every ESP32 variant (the S3 cannot do 16).
// 20 ms / 16384 = 1.2 us per step, about 0.1 deg on an SG90.
#define SERVO_RES LEDC_TIMER_14_BIT
#define SERVO_MAX_DUTY ((1 << 14) - 1)
#define NUM_ARMS 3

static const int   s_pins[NUM_ARMS] = { KK_PIN_SERVO_BOX2, KK_PIN_SERVO_BOX3, KK_PIN_SERVO_BOX4 };
static float s_rest[NUM_ARMS] = KK_ARM_REST_DEG;   // defaults, replaced by saved values
static float s_out[NUM_ARMS]  = KK_ARM_OUT_DEG;
static const int   s_has_arm[KK_NUM_BOXES] = KK_BOX_HAS_ARM;
static bool s_is_out[NUM_ARMS];
static float s_now[NUM_ARMS];      // arm angle last sent (before trim), for the dashboard

// Servo angle that counts as arm angle 0 (flush with the conveyor). Set per
// arm from the dashboard ("Set as 0") and kept in flash; KK_ARM_TRIM_DEG is
// only the first-boot default.
static float s_trim[NUM_ARMS] = KK_ARM_TRIM_DEG;

// Raw servo angle, no trim (used by the isolated servo test).
static void set_angle_raw(int idx, float deg)
{
    if (deg < KK_SERVO_RAW_MIN_DEG) deg = KK_SERVO_RAW_MIN_DEG;
    if (deg > 180.0f) deg = 180.0f;
    float us = KK_SERVO_MIN_US + (KK_SERVO_MAX_US - KK_SERVO_MIN_US) * (deg / 180.0f);
    uint32_t duty = (uint32_t)(us * SERVO_MAX_DUTY / SERVO_PERIOD_US);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0 + idx, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0 + idx);
}

// Arm angle with the per-arm trim, so the same angle puts every arm in the
// same physical position.
static void set_angle(int idx, float deg)
{
    s_now[idx] = deg;
    set_angle_raw(idx, deg + s_trim[idx]);
}

// A hobby servo has no memory of its own, but it always goes to the angle it
// is told. The angles live in flash (NVS), so at power-up each arm is sent
// straight to its saved rest angle.
static void load_angles(void)
{
    nvs_handle_t h;
    if (nvs_open("kk", NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    for (int i = 0; i < NUM_ARMS; i++) {
        char key[24];
        snprintf(key, sizeof key, "arm%d_v3", i + 2);
        float v[2];
        size_t len = sizeof v;
        if (nvs_get_blob(h, key, v, &len) == ESP_OK && len == sizeof v) {
            s_rest[i] = v[0];
            s_out[i] = v[1];
        }
        float z;
        len = sizeof z;
        snprintf(key, sizeof key, "zero%d", i + 2);
        if (nvs_get_blob(h, key, &z, &len) == ESP_OK && len == sizeof z) {
            s_trim[i] = z;
        }
    }
    nvs_close(h);
}

static void save_zero(int idx)
{
    nvs_handle_t h;
    if (nvs_open("kk", NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    char key[24];
    snprintf(key, sizeof key, "zero%d", idx + 2);
    nvs_set_blob(h, key, &s_trim[idx], sizeof s_trim[idx]);
    nvs_commit(h);
    nvs_close(h);
}

// Move an arm to a servo angle without any trim, so the 0 point can be found
// by eye. save = true stores that servo angle as the arm's new 0.
bool arm_zero(int box, float servo_deg, bool save)
{
    int idx = box - 2;
    if (idx < 0 || idx >= NUM_ARMS || servo_deg < 0.0f || servo_deg > 180.0f) {
        return false;
    }
    if (save) {
        s_trim[idx] = servo_deg;
        save_zero(idx);
        s_is_out[idx] = false;
        set_angle(idx, s_rest[idx]);
        ESP_LOGI(TAG, "arm %d: 0 deg is now servo angle %.0f", box, servo_deg);
    } else {
        s_now[idx] = servo_deg - s_trim[idx];
        set_angle_raw(idx, servo_deg);
    }
    return true;
}

float arm_zero_get(int box)
{
    int idx = box - 2;
    return (idx < 0 || idx >= NUM_ARMS) ? 0.0f : s_trim[idx];
}

static void save_angles(int idx)
{
    nvs_handle_t h;
    if (nvs_open("kk", NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    char key[24];
    snprintf(key, sizeof key, "arm%d_v3", idx + 2);
    float v[2] = { s_rest[idx], s_out[idx] };
    nvs_set_blob(h, key, v, sizeof v);
    nvs_commit(h);
    nvs_close(h);
}

void servo_arm_init(void)
{
    load_angles();
    ledc_timer_config_t t = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = SERVO_RES,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = SERVO_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&t));
    for (int i = 0; i < NUM_ARMS; i++) {
        ledc_channel_config_t c = {
            .gpio_num = s_pins[i],
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = LEDC_CHANNEL_0 + i,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = LEDC_TIMER_0,
            .duty = 0,
            .hpoint = 0,
        };
        ESP_ERROR_CHECK(ledc_channel_config(&c));
        gpio_input_enable(s_pins[i]);     // lets arm_test_angle read the pin back
        set_angle(i, s_rest[i]);
        s_is_out[i] = false;
        vTaskDelay(pdMS_TO_TICKS(400));   // one arm at a time: limits the 5 V current peak
    }
    ESP_LOGI(TAG, "3 arms at rest: %.0f / %.0f / %.0f deg", s_rest[0], s_rest[1], s_rest[2]);
}

bool box_has_arm(int box)
{
    return box >= 1 && box <= KK_NUM_BOXES && s_has_arm[box - 1];
}

bool arm_set(int box, bool out)
{
    if (!box_has_arm(box)) {
        return false;
    }
    int idx = box - 2;   // boxes 2, 3, 4 -> 0, 1, 2
    if (idx < 0 || idx >= NUM_ARMS) {
        return false;
    }
    set_angle(idx, out ? s_out[idx] : s_rest[idx]);
    s_is_out[idx] = out;
    ESP_LOGI(TAG, "arm %d %s", box, out ? "out" : "in");
    return true;
}

bool arm_set_out_dir(int box, int dir)
{
    if (!box_has_arm(box)) {
        return false;
    }
    int idx = box - 2;
    if (idx < 0 || idx >= NUM_ARMS) {
        return false;
    }
    bool low = (dir >= 0) == (KK_ARM_OUT_FWD_IS_LOW != 0);
    float deg = low ? s_out[idx] : 180.0f - s_out[idx];
    set_angle(idx, deg);
    s_is_out[idx] = true;
    ESP_LOGI(TAG, "arm %d out to %.0f deg (belt %s)", box, deg, dir >= 0 ? "forward" : "reverse");
    return true;
}

float arm_angle_now(int box)
{
    int idx = box - 2;
    return (idx < 0 || idx >= NUM_ARMS) ? 0.0f : s_now[idx];
}

bool arm_is_out(int box)
{
    int idx = box - 2;
    if (idx < 0 || idx >= NUM_ARMS) {
        return false;
    }
    return s_is_out[idx];
}

bool arm_set_angles(int box, float rest_deg, float out_deg)
{
    int idx = box - 2;
    if (idx < 0 || idx >= NUM_ARMS || rest_deg < 0.0f || rest_deg > 180.0f
        || out_deg < 0.0f || out_deg > 180.0f) {
        return false;
    }
    s_rest[idx] = rest_deg;
    s_out[idx] = out_deg;
    save_angles(idx);
    set_angle(idx, s_is_out[idx] ? s_out[idx] : s_rest[idx]);   // show the result right away
    ESP_LOGI(TAG, "arm %d angles saved: rest %.0f, out %.0f", box, rest_deg, out_deg);
    return true;
}

bool arm_get_angles(int box, float *rest_deg, float *out_deg)
{
    int idx = box - 2;
    if (idx < 0 || idx >= NUM_ARMS) {
        return false;
    }
    *rest_deg = s_rest[idx];
    *out_deg = s_out[idx];
    return true;
}

// Time one high pulse on a pin. Returns 0 if the pin never pulses.
static int measure_pulse_us(int pin)
{
    int64_t t = esp_timer_get_time();
    while (gpio_get_level(pin)) {                 // finish any pulse in progress
        if (esp_timer_get_time() - t > 25000) return 0;
    }
    t = esp_timer_get_time();
    while (!gpio_get_level(pin)) {                // wait for the next rising edge
        if (esp_timer_get_time() - t > 25000) return 0;
    }
    int64_t rise = esp_timer_get_time();
    while (gpio_get_level(pin)) {
        if (esp_timer_get_time() - rise > 5000) return 0;
    }
    return (int)(esp_timer_get_time() - rise);
}

bool arm_test_angle(int box, float deg, int *pulse_us)
{
    int idx = box - 2;
    if (idx < 0 || idx >= NUM_ARMS || deg < 0.0f || deg > 180.0f) {
        return false;
    }
    set_angle_raw(idx, deg);
    vTaskDelay(pdMS_TO_TICKS(60));                // let the new duty take effect
    if (pulse_us) {
        *pulse_us = measure_pulse_us(s_pins[idx]);
    }
    ESP_LOGI(TAG, "test: arm %d (GPIO %d) to %.0f deg, pulse on pin %d us", box, s_pins[idx], deg,
             pulse_us ? *pulse_us : -1);
    return true;
}

void arms_all_in(void)
{
    for (int b = 2; b <= 4; b++) {
        arm_set(b, false);
    }
}
