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
#define KK_SIM_MODE   1
#define KK_SIM_PART_G 40.0f

// ---------------------------------------------------------------------
// Wi-Fi
//   If KK_WIFI_STA_SSID is non-empty the ESP32 first tries to join that
//   network (for example the 2.4 GHz hotspot from the laptop). If it cannot
//   connect within KK_WIFI_STA_TIMEOUT_MS it starts its own hotspot instead.
//   Leave the SSID empty to always start the hotspot.
//   Hotspot address is always http://192.168.4.1
// ---------------------------------------------------------------------
#define KK_WIFI_STA_SSID       ""
#define KK_WIFI_STA_PASS       ""
#define KK_WIFI_STA_TIMEOUT_MS 15000
#define KK_WIFI_AP_SSID        "KineticKitchen"
#define KK_WIFI_AP_PASS        "kinetic123"   // at least 8 characters, or "" for open
#define KK_WIFI_AP_CHANNEL     6

// ---------------------------------------------------------------------
// Pins - PLACEHOLDER (ESP32 WROOM-32 DevKit)
//   Avoid strapping pins 0, 2(LED only), 5, 12, 15 and the flash pins 6-11.
//   Pins 34-39 are input only.
// ---------------------------------------------------------------------
#define KK_PIN_STEP         25   // TMC2209 STEP
#define KK_PIN_DIR          26   // TMC2209 DIR
#define KK_PIN_EN           27   // TMC2209 EN (active low)
#define KK_PIN_SERVO_BOX2   18   // SG90 signal, arm for box 2
#define KK_PIN_SERVO_BOX3   19   // SG90 signal, arm for box 3
#define KK_PIN_SERVO_BOX4   23   // SG90 signal, arm for box 4
#define KK_PIN_HX_BOX2_DOUT 32   // HX711 under box 2
#define KK_PIN_HX_BOX2_SCK  33
#define KK_PIN_HX_BOX4_DOUT 34   // HX711 under box 4 (input-only pin is fine for DOUT)
#define KK_PIN_HX_BOX4_SCK  14
#define KK_PIN_ESTOP        4    // latching stop button to GND (improvement A1)
#define KK_ESTOP_ENABLED    0    // set to 1 once the button is wired
#define KK_PIN_LED          2    // on-board LED: slow blink idle, fast blink while routing

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
#define KK_DIR_FORWARD_LEVEL 1       // DIR pin level that moves the belt toward box 5

// ---------------------------------------------------------------------
// Stepper (NEMA 17 17HS4401, TMC2209 step/dir, GT2 2:1, 30 mm roller)
// ---------------------------------------------------------------------
#define KK_MOTOR_STEPS_PER_REV 200
#define KK_MICROSTEPS          8      // TMC2209 default with MS1=MS2=low (PLACEHOLDER)
#define KK_PULLEY_RATIO        2.0f   // 40T on the roller / 20T on the motor
#define KK_ROLLER_DIAMETER_MM  30.0f
#define KK_BELT_SPEED_MM_S     150.0f
#define KK_BELT_ACCEL_MM_S2    600.0f

// ---------------------------------------------------------------------
// Servo arms (SG90). Angles in degrees, index 0..2 = box 2, 3, 4.
// ---------------------------------------------------------------------
#define KK_ARM_REST_DEG  { 0.0f, 0.0f, 0.0f }     // PLACEHOLDER
#define KK_ARM_OUT_DEG   { 45.0f, 45.0f, 45.0f }  // PLACEHOLDER
#define KK_SERVO_MIN_US  500
#define KK_SERVO_MAX_US  2500
#define KK_ARM_SETTLE_MS 500

// ---------------------------------------------------------------------
// Load cells (1 kg bar cell + HX711). Thresholds from report section 7.3.
// ---------------------------------------------------------------------
#define KK_HX_DEFAULT_SCALE 1000.0f  // HX711 counts per gram, PLACEHOLDER until calibrated
#define KK_OCCUPIED_G       3.0f     // above this for KK_OCCUPIED_MS -> occupied
#define KK_OCCUPIED_MS      1000
#define KK_VACANT_G         2.0f     // below this for KK_VACANT_MS -> vacant, then re-zero
#define KK_VACANT_MS        2000
#define KK_CONFIRM_G        3.0f     // "part landed" while a route is running
#define KK_CONFIRM_MS       300
#define KK_SETTLE_MS        1500     // wait after the belt stops before the final check
#define KK_CELL_SAMPLE_MS   100      // HX711 runs at 10 samples/s

// ---------------------------------------------------------------------
// Routing
// ---------------------------------------------------------------------
#define KK_ROUTE_TIMEOUT_MS 10000
#define KK_LOG_CAPACITY     200
