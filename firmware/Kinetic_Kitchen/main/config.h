#pragma once
// =====================================================================
//  Kinetic Kitchen - firmware configuration
//  Everything marked PLACEHOLDER must be checked against the real wiring
//  and measured on the prototype before the Expo.
// =====================================================================

#define KK_FW_VERSION "0.1.0"

// ---------------------------------------------------------------------
// Simulation
//   1 = the HX711 load cells are faked in software so the dashboard can be
//       demoed before the electronics are wired. The stepper and servo code
//       still runs and toggles the real pins. Routes to boxes 2 and 4 "land"
//       a KK_SIM_PART_G part automatically; the dashboard can place/remove
//       parts by hand.
//   0 = real load cells.
// ---------------------------------------------------------------------
#define KK_SIM_MODE   0
#define KK_SIM_PART_G 40.0f

// ---------------------------------------------------------------------
// Motor connected
//   0 = the stepper and driver are not wired. The driver check, the motor
//       bench test and the TMC2209 UART setup are skipped, and the dashboard
//       refuses routes and jogs. Load cells and servo arms work normally.
//   1 = motor wired: full firmware (and KK_DRIVER_CHECK / KK_MOTOR_TEST apply).
// ---------------------------------------------------------------------
#define KK_MOTOR_CONNECTED 1

// ---------------------------------------------------------------------
// Driver check (takes priority over the motor bench test)
//   TMC2209 only: needs KK_TMC_UART_ENABLED 1. Leave at 0 with the A4988.
//   1 = skip Wi-Fi, servos and load cells. Talk to the TMC2209 over UART and
//       print PASS/FAIL for: UART link, 12 V supply, motor current, the EN /
//       STEP / DIR wires, coil shorts, temperature, then a slow move.
//       After the report the motor keeps moving slowly back and forth.
//   0 = off.
// ---------------------------------------------------------------------
#define KK_DRIVER_CHECK  0

// ---------------------------------------------------------------------
// Motor bench test
//   1 = skip Wi-Fi, servos and load cells. Only the stepper runs:
//       hold 3 s, 50 mm forward, pause, 50 mm back, motor off, repeat.
//   0 = normal firmware.
// ---------------------------------------------------------------------
#define KK_MOTOR_TEST    0
#define KK_TEST_MOVE_MM  50.0f
// Slow, gentle profile for the bench test (belt speed below is untouched).
// 20 mm/s = about 23 motor RPM. Raise once the motor turns cleanly.
#define KK_TEST_SPEED_MM_S  20.0f
#define KK_TEST_ACCEL_MM_S2 100.0f

// ---------------------------------------------------------------------
// Wi-Fi
//   If KK_WIFI_STA_SSID is non-empty the ESP32 first tries to join that
//   network (for example the 2.4 GHz hotspot from the laptop). If it cannot
//   connect within KK_WIFI_STA_TIMEOUT_MS it starts its own hotspot instead.
//   Leave the SSID empty to always start the hotspot.
//   Hotspot address is always http://192.168.4.1
// ---------------------------------------------------------------------
//   Put the real name and password in main/wifi_secrets.h (not committed, the
//   repo is public):
//       #define KK_WIFI_STA_SSID "name"
//       #define KK_WIFI_STA_PASS "password"
#if __has_include("wifi_secrets.h")
#include "wifi_secrets.h"
#endif
#ifndef KK_WIFI_STA_SSID
#define KK_WIFI_STA_SSID       ""
#define KK_WIFI_STA_PASS       ""
#endif
#define KK_WIFI_STA_TIMEOUT_MS 15000
#define KK_WIFI_AP_SSID        "KineticKitchen"
#define KK_WIFI_AP_PASS        "kinetic123"   // at least 8 characters, or "" for open
#define KK_WIFI_AP_CHANNEL     6

// ---------------------------------------------------------------------
// Pins (NodeMCU-32S, ESP32-WROOM-32, 38 pins). Board label IOxx = GPIO xx.
//   Avoid the strapping pins 0, 2 (LED only), 5, 12, 15, the flash pins 6-11,
//   and 1 / 3 (USB serial). Pins 34-39 are input only.
// ---------------------------------------------------------------------
#define KK_PIN_STEP         25   // A4988 STEP
#define KK_PIN_DIR          32   // A4988 DIR
#define KK_PIN_EN           33   // A4988 ENABLE (active low)
#define KK_PIN_SERVO_BOX2   23   // SG90 signal, arm for box 2 (servo 1)
#define KK_PIN_SERVO_BOX3   19   // SG90 signal, arm for box 3 (servo 2)
#define KK_PIN_SERVO_BOX4   18   // SG90 signal, arm for box 4 (servo 3)
#define KK_PIN_HX_BOX2_DOUT 26   // HX711 under box 2 (load cell 1) DT
#define KK_PIN_HX_BOX2_SCK  27   // HX711 under box 2 (load cell 1) SCK
#define KK_PIN_HX_BOX4_DOUT 13   // HX711 under box 4 (load cell 2) DT
#define KK_PIN_HX_BOX4_SCK  14   // HX711 under box 4 (load cell 2) SCK
#define KK_PIN_ESTOP        4    // latching stop button to GND (improvement A1)
#define KK_ESTOP_ENABLED    0    // set to 1 once the button is wired
#define KK_PIN_LED          2    // on-board blue LED: slow blink idle, fast blink while routing
// Only used with a TMC2209 (KK_TMC_UART_ENABLED 1). Not wired with the A4988.
#define KK_PIN_UART_ESP_TX  17
#define KK_PIN_UART_ESP_RX  16

// ---------------------------------------------------------------------
// Belt geometry - PLACEHOLDER (measure on the prototype)
//   Positions in mm along the belt. 0 = box 2 / start 1. Positive toward
//   box 5. Taken from the report: start 1 to box 4 about 369 mm, start 1 to
//   box 5 about 476 mm, so the box pitch is about 184.5 mm and the ends are
//   about 107 mm past the last side box.
// ---------------------------------------------------------------------
#define KK_NUM_BOXES  5
#define KK_NUM_STARTS 3
#define KK_NUM_CELLS  2
#define KK_POS_STARTS { 0.0f, 184.5f, 369.0f }
#define KK_POS_BOXES  { -107.0f, 0.0f, 184.5f, 369.0f, 476.0f }
#define KK_BOX_HAS_ARM  { 0, 1, 1, 1, 0 }     // boxes 2, 3, 4 have a servo arm
#define KK_CELL_BOX     { 2, 4 }              // which box each load cell sits under
#define KK_ARM_CROSS_MM      150.0f  // extra belt travel so the part slides along the arm
#define KK_END_OVERSHOOT_MM  150.0f  // extra belt travel so the part drops off the end
// Belt run multiplier per box (boxes 1..5). Box 3 has no load cell, so the
// belt cannot tell when the part landed; it runs twice the planned distance
// so a part that sticks on the arm still reaches the box.
#define KK_BOX_RUN_FACTOR    { 1.0f, 1.0f, 2.0f, 1.0f, 1.0f }
#define KK_DIR_FORWARD_LEVEL 0       // DIR pin level that moves the belt toward box 5

// ---------------------------------------------------------------------
// Stepper (NEMA 17 StepperOnline 17HS19-2004S1, A4988 step/dir, GT2 2:1, 30 mm roller)
//   A4988 microstepping is set by its MS1/MS2/MS3 pins:
//     all unconnected (internal pull-downs) = 1 (full step)
//     MS1 high = 2, MS2 high = 4, MS1+MS2 high = 8, all three high = 16
//   KK_MICROSTEPS must match, or every distance is off by that factor.
// ---------------------------------------------------------------------
#define KK_MOTOR_STEPS_PER_REV 200
#define KK_MICROSTEPS          1      // MS1-MS3 not wired = full step. CHECK on the driver
#define KK_PULLEY_RATIO        2.0f   // 40T on the roller / 20T on the motor
#define KK_ROLLER_DIAMETER_MM  30.0f
// Measured belt calibration (19 Nov): a 200 mm move went 245 mm and 4 x 50 mm
// went 222 mm. Fit: real = 1.263 x calculated - 7.7 mm per move. The scale
// covers the belt thickness on the roller; the loss is slack/slip at each start.
// To recalibrate: move A mm once and B mm in N moves, measure, refit.
#define KK_BELT_MM_SCALE       1.263f
#define KK_BELT_START_LOSS_MM  7.7f
// Speeds are true belt mm/s now. 95 keeps the same physical speed as before
// the calibration (75 uncalibrated); 150 uncalibrated made the GT2 belt skip.
#define KK_BELT_SPEED_MM_S     95.0f
#define KK_BELT_ACCEL_MM_S2    250.0f

// ---------------------------------------------------------------------
// TMC2209 UART (only for a TMC2209; the A4988 has no UART and its current is
// set with the VREF knob). With it working, motor current is set here and the
// VREF knob on the driver is ignored. MS1 and MS2 unconnected = address 0.
// ---------------------------------------------------------------------
#define KK_TMC_UART_ENABLED 0
#if KK_DRIVER_CHECK && !KK_TMC_UART_ENABLED
#error "KK_DRIVER_CHECK talks to a TMC2209 over UART; it cannot test an A4988"
#endif
#define KK_TMC_UART_PORT    2        // UART2
#define KK_TMC_UART_BAUD    115200
#define KK_TMC_ADDR         0
#define KK_TMC_RSENSE_OHM   0.11f    // BTT TMC2209 sense resistors
#define KK_TMC_RUN_MA       700      // RMS run current; raise in 100 mA steps if it skips
#define KK_TMC_HOLD_PCT     50       // standstill current, percent of run current

// ---------------------------------------------------------------------
// Servo arms (SG90). Angles in degrees, index 0..2 = box 2, 3, 4.
// ---------------------------------------------------------------------
// These are only the first-boot defaults. Tune each arm from the dashboard
// (Bring-up > Arm angles): the values are saved to flash and used from then on,
// so every power-up puts the arms flush with the conveyor at the saved rest angle.
#define KK_ARM_REST_DEG  { 0.0f, 0.0f, 0.0f }
// Out angle when the belt runs forward (toward box 5). Running in reverse the
// arm swings the mirror angle, 180 - out (65 one way -> 115 the other), so it
// always faces the part coming at it. Swap with KK_ARM_OUT_FWD_IS_LOW 0.
#define KK_ARM_OUT_DEG   { 65.0f, 65.0f, 65.0f }
#define KK_ARM_OUT_FWD_IS_LOW 0
// Arm 0 point: the servo angle at which each arm is flush with the conveyor.
// Set it from the dashboard (Bring-up > Arm 0 point: nudge until the arm
// sits where you want, then Set as 0). That is saved in flash and replaces
// these first-boot defaults. Index 0..2 = box 2 (GPIO 23), box 3 (GPIO 19),
// box 4 (GPIO 18). A higher servo angle turns the SG90 counter-clockwise.
// The 0 point must be 65 or less so the 115 deg out position stays in range.
// The isolated servo test on the dashboard sends raw angles (no 0 point).
#define KK_ARM_TRIM_DEG      { 0.0f, 0.0f, 35.0f }
#define KK_SERVO_RAW_MIN_DEG -10.0f
#define KK_SERVO_MIN_US  500
#define KK_SERVO_MAX_US  2500
#define KK_ARM_SETTLE_MS 500

// ---------------------------------------------------------------------
// Load cells (1 kg bar cell + HX711). Thresholds from report section 7.3.
// ---------------------------------------------------------------------
// Sign of each load cell, index 0 = box 2, 1 = box 4. Use -1 when adding weight
// makes the reading go down (cell mounted upside down or A+/A- swapped).
#define KK_HX_SIGN          { -1, 1 }
#define KK_HX_DEFAULT_SCALE 1000.0f  // HX711 counts per gram, PLACEHOLDER until calibrated
#define KK_OCCUPIED_G       3.0f     // above this for KK_OCCUPIED_MS -> occupied
#define KK_OCCUPIED_MS      1000
#define KK_VACANT_G         2.0f     // below this for KK_VACANT_MS -> vacant, then re-zero
#define KK_VACANT_MS        2000
#define KK_CONFIRM_G        3.0f     // "part landed" while a route is running
#define KK_CONFIRM_MS       300
#define KK_SETTLE_MS        1500     // wait after the belt stops before the final check
#define KK_CELL_SAMPLE_MS   100      // HX711 runs at 10 samples/s
// Drift handling. Load cells creep with temperature and time.
//   Empty box:    while the reading stays within +-KK_ZERO_TRACK_G the zero
//                 point follows it continuously, so it never wanders off.
//   Occupied box: the part weight is latched when it lands. Slow creep is
//                 followed and removed, so the shown weight stays put and the
//                 box only goes vacant on a real, sudden drop.
#define KK_ZERO_TRACK_G       1.5f   // must stay below KK_VACANT_G
#define KK_DRIFT_RATE_G_PER_S 0.05f  // fastest creep that is followed (3 g per minute)
#define KK_REMOVED_FRACTION   0.5f   // vacant when less than this share of the part is left

// ---------------------------------------------------------------------
// Routing
// ---------------------------------------------------------------------
#define KK_ROUTE_TIMEOUT_MS 15000   // longest route (start 3 to box 1) is about 6 s at 95 mm/s; box 3 runs 2x
#define KK_LOG_CAPACITY     200
