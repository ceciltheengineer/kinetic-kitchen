#include "router.h"
#include "config.h"
#include "stepper.h"
#include "servo_arm.h"
#include "boxes.h"
#include "hx711.h"
#include "runlog.h"

#include <math.h>
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "router";

typedef enum { CMD_ROUTE, CMD_STOP, CMD_JOG, CMD_ARM, CMD_MOTOR_OFF } cmd_type_t;

typedef struct {
    cmd_type_t type;
    int a;      // start / box / dir
    int b;      // box / out
    float f;    // mm
} cmd_t;

static const float s_starts[KK_NUM_STARTS] = KK_POS_STARTS;
static const float s_boxes[KK_NUM_BOXES]   = KK_POS_BOXES;

static QueueHandle_t s_q;
static SemaphoreHandle_t s_lock;
static router_status_t s_st;
static int64_t s_t0_ms;
static uint32_t s_run_id;
static volatile bool s_busy;

static inline int64_t now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

static void set_state(route_state_t st, const char *msg)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_st.state = st;
    if (msg) {
        strlcpy(s_st.msg, msg, sizeof s_st.msg);
    }
    xSemaphoreGive(s_lock);
    ESP_LOGI(TAG, "[%s] %s", router_state_str(st), msg ? msg : "");
}

// ---- e-stop button ----
static bool estop_pressed(void)
{
#if KK_ESTOP_ENABLED
    return gpio_get_level(KK_PIN_ESTOP) == 0;
#else
    return false;
#endif
}

// Returns RES_OK if nothing interrupts, else the interrupting result.
static run_result_t check_interrupt(void)
{
    if (estop_pressed()) {
        return RES_ESTOP;
    }
    cmd_t c;
    while (xQueueReceive(s_q, &c, 0) == pdTRUE) {
        if (c.type == CMD_STOP) {
            return RES_STOPPED;
        }
        // Any other command during a route is dropped; the web API already
        // refuses them while busy.
    }
    return RES_OK;
}

// Wait ms, bailing out early on stop. Returns the interrupt result.
static run_result_t wait_ms(uint32_t ms)
{
    int64_t until = now_ms() + ms;
    while (now_ms() < until) {
        run_result_t r = check_interrupt();
        if (r != RES_OK) {
            return r;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    return RES_OK;
}

static void emergency_halt(void)
{
    stepper_abort();
    arms_all_in();
    stepper_enable(false);
}

// ---- the route itself ----
static void do_route(int start, int box)
{
    run_entry_t e = {
        .id = ++s_run_id,
        .t_start_ms = now_ms(),
        .start = start,
        .box = box,
        .g_before = boxes_grams(box),
    };
    s_t0_ms = e.t_start_ms;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_st.start = start;
    s_st.box = box;
    s_st.run_id = e.id;
    s_st.expected_mm = 0.0f;
    xSemaphoreGive(s_lock);

    char msg[64];
    if (!boxes_available(box)) {
        snprintf(msg, sizeof msg, "Box %d is %s", box, boxes_state_str(boxes_state(box)));
        set_state(RS_ERROR, msg);
        e.result = RES_REJECTED;
        runlog_add(&e);
        return;
    }

    bool  has_arm  = box_has_arm(box);
    bool  has_cell = boxes_has_cell(box);
    float from = s_starts[start - 1];
    float to   = s_boxes[box - 1];
    int   dir  = (to - from) >= 0.0f ? 1 : -1;
    float dist = fabsf(to - from);
    float expected = dist + (has_arm ? KK_ARM_CROSS_MM : KK_END_OVERSHOOT_MM);

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_st.expected_mm = expected;
    xSemaphoreGive(s_lock);

    run_result_t result = RES_OK;

    // 2. Set the arm.
    snprintf(msg, sizeof msg, "Start %d to box %d: %s", start, box,
             has_arm ? "arm out" : "arms stay in");
    set_state(RS_ARM_OUT, msg);
    if (has_arm) {
        arm_set_out_dir(box, dir);
        result = wait_ms(KK_ARM_SETTLE_MS);
    }

    // 3. Move.
    if (result == RES_OK) {
        snprintf(msg, sizeof msg, "Belt %s, %.0f mm%s", dir > 0 ? "forward" : "reverse",
                 expected, has_cell ? ", waiting for weight" : "");
        set_state(RS_RUNNING, msg);
        // With a load cell the belt keeps going until the weight confirms,
        // bounded by the timeout. Without one it runs the planned distance.
        float run_mm = has_cell
            ? expected + KK_BELT_SPEED_MM_S * (KK_ROUTE_TIMEOUT_MS / 1000.0f)
            : expected;
        if (!stepper_move_mm(run_mm, dir)) {
            set_state(RS_ERROR, "Belt did not start");
            result = RES_REJECTED;
        }
    }

    int64_t above_since = 0;
#if KK_SIM_MODE
    bool sim_dropped = false;
#endif
    while (result == RES_OK) {
        run_result_t irq = check_interrupt();
        if (irq != RES_OK) {
            result = irq;
            if (irq == RES_ESTOP) emergency_halt(); else stepper_stop();
            break;
        }
        int64_t t = now_ms();
        if (t - s_t0_ms > KK_ROUTE_TIMEOUT_MS) {
            stepper_stop();
            result = RES_TIMEOUT;
            break;
        }
        if (has_cell) {
#if KK_SIM_MODE
            if (!sim_dropped && stepper_mm_moved() >= expected) {
                hx711_sim_set_grams(boxes_cell_index(box), KK_SIM_PART_G);
                sim_dropped = true;
            }
#endif
            if (boxes_grams(box) > KK_CONFIRM_G) {
                if (above_since == 0) {
                    above_since = t;
                } else if (t - above_since >= KK_CONFIRM_MS) {
                    stepper_stop();   // 4. Stop: the part has landed
                    break;
                }
            } else {
                above_since = 0;
            }
            if (!stepper_is_moving()) {
                result = RES_TIMEOUT;   // ran out of belt travel
                break;
            }
        } else if (!stepper_is_moving()) {
            break;   // 4. Stop: planned distance done
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    // Let the belt decelerate.
    while (stepper_is_moving()) {
        if (estop_pressed()) {
            emergency_halt();
            result = RES_ESTOP;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    e.mm_moved = stepper_mm_moved();

    // Settle and confirm the weight.
    if (result == RES_OK && has_cell) {
        set_state(RS_SETTLING, "Belt stopped, checking weight");
        run_result_t irq = wait_ms(KK_SETTLE_MS);
        if (irq != RES_OK) {
            result = irq;
        } else if (boxes_grams(box) < KK_CONFIRM_G) {
            result = RES_NOT_CONFIRMED;
        }
    }

    // Arm back.
    set_state(RS_ARM_IN, "Arm back");
    if (has_arm) {
        arm_set(box, false);
        vTaskDelay(pdMS_TO_TICKS(KK_ARM_SETTLE_MS));
    }
    stepper_enable(false);   // motor off between prints

    e.result = result;
    e.duration_ms = (uint32_t)(now_ms() - s_t0_ms);
    e.g_after = boxes_grams(box);
    runlog_add(&e);

    switch (result) {
    case RES_OK:
        snprintf(msg, sizeof msg, "Delivered to box %d in %.1f s", box, e.duration_ms / 1000.0f);
        set_state(RS_DONE, msg);
        break;
    case RES_TIMEOUT:
        if (has_cell) boxes_set_needs_check(box);
        snprintf(msg, sizeof msg, "Timeout: box %d did not confirm in %d s", box, KK_ROUTE_TIMEOUT_MS / 1000);
        set_state(RS_ERROR, msg);
        break;
    case RES_NOT_CONFIRMED:
        boxes_set_needs_check(box);
        snprintf(msg, sizeof msg, "Weight on box %d did not settle, check it", box);
        set_state(RS_ERROR, msg);
        break;
    case RES_STOPPED:
        set_state(RS_STOPPED, "Stopped by user");
        break;
    case RES_ESTOP:
        set_state(RS_ESTOP, "Emergency stop pressed");
        break;
    default:
        set_state(RS_ERROR, "Route failed");
        break;
    }
}

static void do_jog(float mm, int dir)
{
    char msg[64];
    snprintf(msg, sizeof msg, "Jog %.0f mm %s", mm, dir > 0 ? "forward" : "reverse");
    set_state(RS_RUNNING, msg);
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_st.start = 0;
    s_st.box = 0;
    s_st.expected_mm = mm;
    xSemaphoreGive(s_lock);
    s_t0_ms = now_ms();
    if (!stepper_move_mm(mm, dir)) {
        set_state(RS_ERROR, "Belt did not start");
        return;
    }
    run_result_t r = RES_OK;
    while (stepper_is_moving()) {
        r = check_interrupt();
        if (r == RES_ESTOP) { emergency_halt(); break; }
        if (r == RES_STOPPED) { stepper_stop(); }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    while (stepper_is_moving()) vTaskDelay(pdMS_TO_TICKS(10));
    stepper_enable(false);
    set_state(r == RES_ESTOP ? RS_ESTOP : RS_IDLE, "Jog done");
}

static void router_task(void *arg)
{
    cmd_t c;
    for (;;) {
        if (xQueueReceive(s_q, &c, pdMS_TO_TICKS(50)) == pdTRUE) {
            switch (c.type) {
            case CMD_ROUTE:
                s_busy = true;
                do_route(c.a, c.b);
                s_busy = false;
                break;
            case CMD_JOG:
                s_busy = true;
                do_jog(c.f, c.a);
                s_busy = false;
                break;
            case CMD_ARM:
                arm_set(c.a, c.b != 0);
                break;
            case CMD_MOTOR_OFF:
                stepper_enable(false);
                break;
            case CMD_STOP:
                break;   // nothing running
            }
        }
        // E-stop while idle: keep everything off and show it.
        bool es = estop_pressed();
        xSemaphoreTake(s_lock, portMAX_DELAY);
        bool was = s_st.estop;
        s_st.estop = es;
        xSemaphoreGive(s_lock);
        if (es && !was) {
            emergency_halt();
            set_state(RS_ESTOP, "Emergency stop pressed");
        } else if (!es && was) {
            set_state(RS_IDLE, "Emergency stop released");
        }
        stepper_is_moving();   // timer housekeeping
    }
}

void router_init(void)
{
#if KK_ESTOP_ENABLED
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << KK_PIN_ESTOP,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
#endif
    s_lock = xSemaphoreCreateMutex();
    s_q = xQueueCreate(8, sizeof(cmd_t));
    memset(&s_st, 0, sizeof s_st);
    strlcpy(s_st.msg, "Ready", sizeof s_st.msg);
    xTaskCreatePinnedToCore(router_task, "router", 4096, NULL, 6, NULL, 1);
}

bool router_busy(void)
{
    return s_busy;
}

void router_get_status(router_status_t *out)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *out = s_st;
    xSemaphoreGive(s_lock);
    out->elapsed_ms = s_busy ? (uint32_t)(now_ms() - s_t0_ms) : 0;
}

bool router_route(int start, int box)
{
    if (s_busy || start < 1 || start > KK_NUM_STARTS || box < 1 || box > KK_NUM_BOXES) {
        return false;
    }
    cmd_t c = { .type = CMD_ROUTE, .a = start, .b = box };
    return xQueueSend(s_q, &c, 0) == pdTRUE;
}

void router_stop(void)
{
    cmd_t c = { .type = CMD_STOP };
    xQueueSend(s_q, &c, 0);
}

bool router_jog(float mm, int dir)
{
    if (s_busy || mm <= 0.0f || mm > 1000.0f) {
        return false;
    }
    cmd_t c = { .type = CMD_JOG, .a = dir, .f = mm };
    return xQueueSend(s_q, &c, 0) == pdTRUE;
}

bool router_arm(int box, bool out)
{
    if (s_busy || !box_has_arm(box)) {
        return false;
    }
    cmd_t c = { .type = CMD_ARM, .a = box, .b = out ? 1 : 0 };
    return xQueueSend(s_q, &c, 0) == pdTRUE;
}

bool router_motor_off(void)
{
    if (s_busy) {
        return false;
    }
    cmd_t c = { .type = CMD_MOTOR_OFF };
    return xQueueSend(s_q, &c, 0) == pdTRUE;
}

float router_start_pos(int start)
{
    return (start >= 1 && start <= KK_NUM_STARTS) ? s_starts[start - 1] : 0.0f;
}

float router_box_pos(int box)
{
    return (box >= 1 && box <= KK_NUM_BOXES) ? s_boxes[box - 1] : 0.0f;
}

const char *router_state_str(route_state_t s)
{
    switch (s) {
    case RS_IDLE:     return "idle";
    case RS_ARM_OUT:  return "arm_out";
    case RS_RUNNING:  return "running";
    case RS_SETTLING: return "settling";
    case RS_ARM_IN:   return "arm_in";
    case RS_DONE:     return "done";
    case RS_ERROR:    return "error";
    case RS_STOPPED:  return "stopped";
    case RS_ESTOP:    return "estop";
    }
    return "?";
}
