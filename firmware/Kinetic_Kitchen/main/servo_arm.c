#include "servo_arm.h"
#include "config.h"

#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "servo";

#define SERVO_FREQ_HZ 50
#define SERVO_PERIOD_US 20000
#define SERVO_RES LEDC_TIMER_16_BIT
#define SERVO_MAX_DUTY ((1 << 16) - 1)
#define NUM_ARMS 3

static const int   s_pins[NUM_ARMS] = { KK_PIN_SERVO_BOX2, KK_PIN_SERVO_BOX3, KK_PIN_SERVO_BOX4 };
static const float s_rest[NUM_ARMS] = KK_ARM_REST_DEG;
static const float s_out[NUM_ARMS]  = KK_ARM_OUT_DEG;
static const int   s_has_arm[KK_NUM_BOXES] = KK_BOX_HAS_ARM;
static bool s_is_out[NUM_ARMS];

static void set_angle(int idx, float deg)
{
    if (deg < 0.0f) deg = 0.0f;
    if (deg > 180.0f) deg = 180.0f;
    float us = KK_SERVO_MIN_US + (KK_SERVO_MAX_US - KK_SERVO_MIN_US) * (deg / 180.0f);
    uint32_t duty = (uint32_t)(us * SERVO_MAX_DUTY / SERVO_PERIOD_US);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0 + idx, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0 + idx);
}

void servo_arm_init(void)
{
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
        set_angle(i, s_rest[i]);
        s_is_out[i] = false;
    }
    ESP_LOGI(TAG, "3 arms at rest");
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

bool arm_is_out(int box)
{
    int idx = box - 2;
    if (idx < 0 || idx >= NUM_ARMS) {
        return false;
    }
    return s_is_out[idx];
}

void arms_all_in(void)
{
    for (int b = 2; b <= 4; b++) {
        arm_set(b, false);
    }
}
