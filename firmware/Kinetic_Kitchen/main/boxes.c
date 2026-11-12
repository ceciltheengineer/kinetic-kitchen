#include "boxes.h"
#include "hx711.h"

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "boxes";

#define TARE_SAMPLES 8

typedef struct {
    box_info_t info;
    int64_t above_since_ms;
    int64_t below_since_ms;
    bool    tare_req;
    bool    cal_req;
    float   cal_g;
    int     acc_n;
    int64_t acc_sum;
} box_t;

static box_t s_boxes[KK_NUM_BOXES];
static SemaphoreHandle_t s_lock;
static const int s_cell_box[KK_NUM_CELLS] = KK_CELL_BOX;

static inline int64_t now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

static box_t *get(int box)
{
    if (box < 1 || box > KK_NUM_BOXES) {
        return NULL;
    }
    return &s_boxes[box - 1];
}

// ---- NVS: the calibration scale survives reboots ----
static void nvs_load_scale(box_t *b)
{
    nvs_handle_t h;
    if (nvs_open("kk", NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    char key[12];
    snprintf(key, sizeof key, "scale%d", b->info.id);
    float v;
    size_t len = sizeof v;
    if (nvs_get_blob(h, key, &v, &len) == ESP_OK && len == sizeof v && v > 0.0f) {
        b->info.scale = v;
        ESP_LOGI(TAG, "box %d scale from NVS: %.2f counts/g", b->info.id, v);
    }
    nvs_close(h);
}

static void nvs_save_scale(box_t *b)
{
    nvs_handle_t h;
    if (nvs_open("kk", NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    char key[12];
    snprintf(key, sizeof key, "scale%d", b->info.id);
    nvs_set_blob(h, key, &b->info.scale, sizeof b->info.scale);
    nvs_commit(h);
    nvs_close(h);
}

// ---- state machine for one box with a cell (called with the lock held) ----
static void update_box(box_t *b, int32_t raw)
{
    box_info_t *i = &b->info;
    i->raw = raw;

    if (b->tare_req || b->cal_req) {
        i->taring = true;
        b->acc_sum += raw;
        b->acc_n++;
        if (b->acc_n >= TARE_SAMPLES) {
            int32_t avg = (int32_t)(b->acc_sum / b->acc_n);
            if (b->cal_req) {
                float counts = (float)(avg - i->offset);
                if (b->cal_g > 0.0f && counts > 0.0f) {
                    i->scale = counts / b->cal_g;
                    nvs_save_scale(b);
                    ESP_LOGI(TAG, "box %d calibrated: %.2f counts/g", i->id, i->scale);
                } else {
                    ESP_LOGW(TAG, "box %d calibration rejected (counts %.0f)", i->id, counts);
                }
            } else {
                i->offset = avg;
                i->grams = 0.0f;
                ESP_LOGI(TAG, "box %d tared, offset %ld", i->id, (long)avg);
            }
            b->tare_req = b->cal_req = false;
            b->acc_n = 0;
            b->acc_sum = 0;
            b->above_since_ms = b->below_since_ms = 0;
            i->taring = false;
        }
        return;
    }

    float g = (float)(raw - i->offset) / i->scale;
    i->grams = 0.5f * i->grams + 0.5f * g;   // light smoothing
    int64_t t = now_ms();

    switch (i->state) {
    case BOX_FREE:
        if (i->grams > KK_OCCUPIED_G) {
            if (b->above_since_ms == 0) {
                b->above_since_ms = t;
            } else if (t - b->above_since_ms >= KK_OCCUPIED_MS) {
                i->state = BOX_OCCUPIED;
                b->below_since_ms = 0;
                ESP_LOGI(TAG, "box %d OCCUPIED (%.1f g)", i->id, i->grams);
            }
        } else {
            b->above_since_ms = 0;
        }
        break;
    case BOX_OCCUPIED:
        if (i->grams < KK_VACANT_G) {
            if (b->below_since_ms == 0) {
                b->below_since_ms = t;
            } else if (t - b->below_since_ms >= KK_VACANT_MS) {
                i->state = BOX_FREE;
                b->above_since_ms = 0;
                b->tare_req = true;   // re-zero: load cells drift
                ESP_LOGI(TAG, "box %d VACANT, re-zeroing", i->id);
            }
        } else {
            b->below_since_ms = 0;
        }
        break;
    case BOX_NEEDS_CHECK:
        break;   // stays until someone clears it
    }
}

static void boxes_task(void *arg)
{
    for (;;) {
        for (int c = 0; c < KK_NUM_CELLS; c++) {
            int32_t raw;
            if (!hx711_read_raw(c, &raw)) {
                continue;
            }
            xSemaphoreTake(s_lock, portMAX_DELAY);
            update_box(get(s_cell_box[c]), raw);
            xSemaphoreGive(s_lock);
        }
        vTaskDelay(pdMS_TO_TICKS(KK_CELL_SAMPLE_MS));
    }
}

void boxes_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    for (int i = 0; i < KK_NUM_BOXES; i++) {
        box_t *b = &s_boxes[i];
        memset(b, 0, sizeof *b);
        b->info.id = i + 1;
        b->info.cell = -1;
        b->info.scale = KK_HX_DEFAULT_SCALE;
        b->info.state = BOX_FREE;
    }
    for (int c = 0; c < KK_NUM_CELLS; c++) {
        box_t *b = get(s_cell_box[c]);
        b->info.has_cell = true;
        b->info.cell = c;
        b->tare_req = true;   // zero at boot
        nvs_load_scale(b);
    }
    hx711_init();
    // Core 1: the HX711 clock burst runs in a critical section, and the
    // stepper ISR lives on core 0.
    xTaskCreatePinnedToCore(boxes_task, "boxes", 4096, NULL, 5, NULL, 1);
}

void boxes_get(box_info_t out[KK_NUM_BOXES])
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (int i = 0; i < KK_NUM_BOXES; i++) {
        out[i] = s_boxes[i].info;
    }
    xSemaphoreGive(s_lock);
}

box_state_t boxes_state(int box)
{
    box_t *b = get(box);
    if (!b) return BOX_NEEDS_CHECK;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    box_state_t s = b->info.state;
    xSemaphoreGive(s_lock);
    return s;
}

float boxes_grams(int box)
{
    box_t *b = get(box);
    if (!b) return 0.0f;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    float g = b->info.grams;
    xSemaphoreGive(s_lock);
    return g;
}

bool boxes_has_cell(int box)
{
    box_t *b = get(box);
    return b && b->info.has_cell;
}

int boxes_cell_index(int box)
{
    box_t *b = get(box);
    return b ? b->info.cell : -1;
}

bool boxes_available(int box)
{
    box_t *b = get(box);
    if (!b) return false;
    if (!b->info.has_cell) return true;
    return boxes_state(box) == BOX_FREE;
}

void boxes_tare(int box)
{
    box_t *b = get(box);
    if (!b || !b->info.has_cell) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    b->tare_req = true;
    b->cal_req = false;
    b->acc_n = 0;
    b->acc_sum = 0;
    b->info.state = BOX_FREE;
    xSemaphoreGive(s_lock);
}

void boxes_calibrate(int box, float known_g)
{
    box_t *b = get(box);
    if (!b || !b->info.has_cell || known_g <= 0.0f) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    b->cal_req = true;
    b->tare_req = false;
    b->cal_g = known_g;
    b->acc_n = 0;
    b->acc_sum = 0;
    xSemaphoreGive(s_lock);
}

void boxes_set_needs_check(int box)
{
    box_t *b = get(box);
    if (!b || !b->info.has_cell) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    b->info.state = BOX_NEEDS_CHECK;
    xSemaphoreGive(s_lock);
    ESP_LOGW(TAG, "box %d NEEDS CHECK", box);
}

void boxes_clear_check(int box)
{
    box_t *b = get(box);
    if (!b || !b->info.has_cell) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (b->info.state == BOX_NEEDS_CHECK) {
        // Decide from the current weight, like a fresh start.
        b->info.state = (b->info.grams > KK_OCCUPIED_G) ? BOX_OCCUPIED : BOX_FREE;
        b->above_since_ms = b->below_since_ms = 0;
    }
    xSemaphoreGive(s_lock);
}

const char *boxes_state_str(box_state_t s)
{
    switch (s) {
    case BOX_FREE:        return "free";
    case BOX_OCCUPIED:    return "occupied";
    case BOX_NEEDS_CHECK: return "check";
    }
    return "?";
}
