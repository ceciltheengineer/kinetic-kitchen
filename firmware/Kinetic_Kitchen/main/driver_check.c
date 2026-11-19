#include "driver_check.h"
#include "config.h"
#include "tmc_uart.h"
#include "stepper.h"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "check";

// IOIN: the driver's own view of its input pins.
#define IOIN_ENN  (1u << 0)
#define IOIN_MS1  (1u << 2)
#define IOIN_MS2  (1u << 3)
#define IOIN_STEP (1u << 7)
#define IOIN_DIR  (1u << 9)

// DRV_STATUS flags.
#define DS_OTPW  (1u << 0)
#define DS_OT    (1u << 1)
#define DS_S2GA  (1u << 2)
#define DS_S2GB  (1u << 3)
#define DS_S2VSA (1u << 4)
#define DS_S2VSB (1u << 5)
#define DS_OLA   (1u << 6)
#define DS_OLB   (1u << 7)
#define DS_SHORTS (DS_S2GA | DS_S2GB | DS_S2VSA | DS_S2VSB)
#define DS_CS(s) (((s) >> 16) & 0x1F)
#define READ_FAILED 0xFFFFFFFFu

static int s_fail;

static void pass(const char *what)
{
    ESP_LOGI(TAG, "PASS  %s", what);
}

static void fail(const char *what, const char *hint)
{
    s_fail++;
    ESP_LOGE(TAG, "FAIL  %s", what);
    if (hint) {
        ESP_LOGE(TAG, "      -> %s", hint);
    }
}

// LED: solid while everything has passed, fast blink once anything failed.
static void led_wait(int ms)
{
    for (int t = 0; t < ms; t += 100) {
        gpio_set_level(KK_PIN_LED, s_fail ? ((t / 100) & 1) : 1);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void link_hint(tmc_result_t r)
{
    switch (r) {
    case TMC_NO_ECHO:
        ESP_LOGE(TAG, "      -> The ESP32 did not hear its own message. GPIO%d (through the resistor) "
                 "and GPIO%d must meet at the same driver UART pin.", KK_PIN_UART_ESP_TX, KK_PIN_UART_ESP_RX);
        break;
    case TMC_BAD_ECHO:
        ESP_LOGE(TAG, "      -> The ESP32 heard a garbled copy of its message. Check the resistor value "
                 "(1 kOhm) and that ESP32 GND is joined to driver GND.");
        break;
    case TMC_NO_REPLY:
        ESP_LOGE(TAG, "      -> ESP32 side is wired right but the driver stayed silent. The driver needs 12 V on VM "
                 "to talk, not just VIO. Also check VIO = 3.3 V, MS1/MS2 unconnected, and that the wire is on the "
                 "driver UART pin (labelled PDN, RX or TX).");
        break;
    case TMC_BAD_REPLY:
        ESP_LOGE(TAG, "      -> The driver answered but the reply was garbled. Check GND between the boards "
                 "and keep the UART wire short.");
        break;
    default:
        break;
    }
}

static bool check_link(void)
{
    uint32_t ioin = 0;
    tmc_result_t r = tmc_read(TMC_REG_IOIN, &ioin);
    if (r == TMC_NO_REPLY) {
        int a = tmc_find_address();
        if (a > 0) {
            ESP_LOGW(TAG, "WARN  driver answered at address %d, not %d. MS1 or MS2 is pulled high", a, KK_TMC_ADDR);
            r = tmc_read(TMC_REG_IOIN, &ioin);
        }
    }
    if (r != TMC_OK) {
        ESP_LOGE(TAG, "FAIL  driver does not answer over UART (%s)", tmc_result_str(r));
        link_hint(r);
        return false;
    }
    unsigned ver = ioin >> 24;
    if (ver != 0x21) {
        ESP_LOGE(TAG, "FAIL  driver answered with chip version 0x%02x, a TMC2209 is 0x21", ver);
        return false;
    }
    pass("driver answers over UART, chip version 0x21 = TMC2209");
    return true;
}

static void check_gstat(void)
{
    uint32_t g = 0;
    if (tmc_read(TMC_REG_GSTAT, &g) != TMC_OK) {
        fail("read driver status flags", NULL);
        return;
    }
    if (g & 0x4) {
        fail("charge pump undervoltage", "12 V on VM is missing or sagging. Measure VM to GND.");
    } else if (g & 0x2) {
        fail("driver shut down after an earlier error", "A short or overheat happened since power-up.");
    } else {
        pass("no stored driver errors, 12 V supply looks fine");
    }
    tmc_write(TMC_REG_GSTAT, 0x7);   // clear the latched flags
}

// Toggle an ESP32 pin and check the driver sees both levels. Ends at rest.
static bool pin_follows(int gpio, uint32_t bit, int rest)
{
    bool ok = true;
    int levels[2] = { !rest, rest };
    for (int i = 0; i < 2; i++) {
        gpio_set_level(gpio, levels[i]);
        vTaskDelay(pdMS_TO_TICKS(5));
        uint32_t ioin = 0;
        if (tmc_read(TMC_REG_IOIN, &ioin) != TMC_OK) {
            return false;
        }
        if (((ioin & bit) != 0) != (levels[i] != 0)) {
            ok = false;
        }
    }
    return ok;
}

static void check_pins(void)
{
    char msg[112];
    struct { const char *name; int gpio; uint32_t bit; int rest; } pins[] = {
        { "EN",   KK_PIN_EN,   IOIN_ENN,  1 },   // 1 = driver off
        { "STEP", KK_PIN_STEP, IOIN_STEP, 0 },
        { "DIR",  KK_PIN_DIR,  IOIN_DIR,  KK_DIR_FORWARD_LEVEL },
    };
    for (int i = 0; i < 3; i++) {
        if (pin_follows(pins[i].gpio, pins[i].bit, pins[i].rest)) {
            snprintf(msg, sizeof msg, "%s wire: driver sees GPIO%d go high and low", pins[i].name, pins[i].gpio);
            pass(msg);
        } else {
            snprintf(msg, sizeof msg, "%s wire: driver does not follow GPIO%d", pins[i].name, pins[i].gpio);
            fail(msg, "Check that wire end to end, and that it sits on the right driver pin.");
        }
    }
    stepper_enable(false);   // keep the stepper module in sync with the EN pin

    uint32_t ioin = 0;
    if (tmc_read(TMC_REG_IOIN, &ioin) == TMC_OK) {
        int ms1 = (ioin & IOIN_MS1) != 0;
        int ms2 = (ioin & IOIN_MS2) != 0;
        static const int table[4] = { 8, 32, 64, 16 };   // index = MS2 * 2 + MS1
        int micro = table[ms2 * 2 + ms1];
        if (micro == KK_MICROSTEPS) {
            snprintf(msg, sizeof msg, "MS1=%d MS2=%d -> %d microsteps, matches the firmware", ms1, ms2, micro);
            pass(msg);
        } else {
            snprintf(msg, sizeof msg, "MS1=%d MS2=%d -> %d microsteps, firmware expects %d",
                     ms1, ms2, micro, KK_MICROSTEPS);
            fail(msg, "Leave MS1 and MS2 unconnected, or change KK_MICROSTEPS. Distances will be wrong otherwise.");
        }
    }
}

static uint32_t status(void)
{
    uint32_t s = 0;
    return (tmc_read(TMC_REG_DRV_STATUS, &s) == TMC_OK) ? s : READ_FAILED;
}

static void judge_status(const char *phase, uint32_t s, bool moving)
{
    char msg[112];
    if (s == READ_FAILED) {
        snprintf(msg, sizeof msg, "%s: could not read driver status", phase);
        fail(msg, NULL);
        return;
    }
    if (s & (DS_S2GA | DS_S2VSA)) {
        snprintf(msg, sizeof msg, "%s: short on coil A (green/black)", phase);
        fail(msg, "Power off now. Check the A1/A2 wires for a stray strand touching GND, 12 V or each other.");
    }
    if (s & (DS_S2GB | DS_S2VSB)) {
        snprintf(msg, sizeof msg, "%s: short on coil B (red/blue)", phase);
        fail(msg, "Power off now. Check the B1/B2 wires for a stray strand touching GND, 12 V or each other.");
    }
    if (s & DS_OT) {
        snprintf(msg, sizeof msg, "%s: driver overheated and shut down", phase);
        fail(msg, "Lower KK_TMC_RUN_MA or add a heatsink.");
    } else if (s & DS_OTPW) {
        ESP_LOGW(TAG, "WARN  %s: driver is getting hot (over 120 C warning)", phase);
    }
    if (moving && (s & (DS_OLA | DS_OLB))) {
        ESP_LOGW(TAG, "WARN  %s: driver suspects an open coil %s%s. If the motor turned smoothly, ignore this.",
                 phase, (s & DS_OLA) ? "A " : "", (s & DS_OLB) ? "B" : "");
    }
    if (!(s & DS_SHORTS) && !(s & DS_OT)) {
        snprintf(msg, sizeof msg, "%s: no shorts, no overheating (current scale %lu/31)", phase,
                 (unsigned long)DS_CS(s));
        pass(msg);
    }
}

static void move_and_watch(int dir, uint32_t *flags)
{
    stepper_move_mm(KK_TEST_MOVE_MM, dir);
    while (stepper_is_moving()) {
        uint32_t s = status();
        if (s != READ_FAILED) {
            *flags |= s;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void driver_check_run(void)
{
    ESP_LOGW(TAG, "===== TMC2209 DRIVER CHECK =====");
    ESP_LOGW(TAG, "UART: ESP TX GPIO%d -> 1k -> driver UART pin <- ESP RX GPIO%d",
             KK_PIN_UART_ESP_TX, KK_PIN_UART_ESP_RX);
    stepper_init();   // EN high: driver off
    tmc_uart_init();

    // 1. Talk to the driver. Retry so the wiring can be fixed while it runs.
    ESP_LOGI(TAG, "--- 1. UART link");
    while (!check_link()) {
        ESP_LOGW(TAG, "retrying in 3 s");
        for (int i = 0; i < 15; i++) {
            gpio_set_level(KK_PIN_LED, i & 1);
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }

    ESP_LOGI(TAG, "--- 2. Power and stored errors");
    check_gstat();

    ESP_LOGI(TAG, "--- 3. Motor current (set in firmware, knob ignored)");
    float ma = 0;
    tmc_result_t r = tmc_set_current(KK_TMC_RUN_MA, KK_TMC_HOLD_PCT, &ma);
    if (r == TMC_OK) {
        char msg[112];
        snprintf(msg, sizeof msg, "run current %.0f mA, hold %d%%, confirmed by the driver", ma, KK_TMC_HOLD_PCT);
        pass(msg);
    } else {
        fail("set motor current", tmc_result_str(r));
    }

    ESP_LOGI(TAG, "--- 4. Signal wires (driver stays off)");
    check_pins();

    ESP_LOGI(TAG, "--- 5. Holding test");
    stepper_enable(true);
    vTaskDelay(pdMS_TO_TICKS(300));
    uint32_t ioin = 0;
    if (tmc_read(TMC_REG_IOIN, &ioin) == TMC_OK && (ioin & IOIN_ENN)) {
        fail("driver still off after EN went low", "Check the EN wire.");
    }
    judge_status("holding", status(), false);
    ESP_LOGW(TAG, ">>> Motor is ON and holding for 5 s. Try turning the shaft: it should be stiff.");
    led_wait(5000);
    judge_status("after 5 s hold", status(), false);

    ESP_LOGI(TAG, "--- 6. Slow move");
    float turns = KK_TEST_MOVE_MM * stepper_steps_per_mm() / (float)(KK_MOTOR_STEPS_PER_REV * KK_MICROSTEPS);
    ESP_LOGW(TAG, ">>> Moving forward then back, about %.1f motor turns each way at %.0f mm/s",
             turns, KK_TEST_SPEED_MM_S);
    uint32_t flags = 0;
    move_and_watch(+1, &flags);
    led_wait(1000);
    move_and_watch(-1, &flags);
    judge_status("moving", flags, true);
    stepper_enable(false);

    ESP_LOGW(TAG, "===== RESULT: %s (%d failed) =====", s_fail ? "PROBLEMS FOUND" : "ALL CHECKS PASSED", s_fail);
    if (s_fail == 0) {
        ESP_LOGW(TAG, "The driver works and the current is set. Next: set KK_DRIVER_CHECK 0 in config.h.");
    }

    // Keep going so the motor can be watched. Re-apply current if 12 V dropped.
    for (int cycle = 1;; cycle++) {
        led_wait(3000);
        uint32_t g = 0;
        if (tmc_read(TMC_REG_GSTAT, &g) == TMC_OK && (g & 0x1)) {
            ESP_LOGW(TAG, "driver restarted (12 V dropped?). Setting current again");
            tmc_write(TMC_REG_GSTAT, 0x7);
            tmc_set_current(KK_TMC_RUN_MA, KK_TMC_HOLD_PCT, NULL);
        }
        flags = 0;
        stepper_enable(true);
        led_wait(1000);
        move_and_watch(+1, &flags);
        led_wait(1000);
        move_and_watch(-1, &flags);
        stepper_enable(false);
        bool bad = (flags & (DS_SHORTS | DS_OT)) != 0;
        ESP_LOGI(TAG, "cycle %d: moved %.0f mm each way, %s", cycle, KK_TEST_MOVE_MM,
                 bad ? "FAULT FLAGS SET" : "no faults");
        if (bad) {
            judge_status("moving", flags, true);
        }
    }
}
