# Kinetic Kitchen firmware

ESP32 (NodeMCU-32S) firmware and live dashboard for the sorting conveyor prototype
(3 start positions, 5 boxes, reversible belt, 3 servo arms, 2 load cells).
Built with ESP-IDF v6.1 in VS Code.

## What it does

* Serves the dashboard at `http://192.168.4.1` (hotspot) or at the IP it gets
  from the laptop hotspot.
* Runs a route: pick a start position and a box, the arm for that box swings
  out, the belt runs toward the box, the belt stops when the load cell sees the
  part land (boxes 2 and 4) or after the planned distance (boxes 1, 3, 5), the
  arm comes back and the motor is released. A route is aborted after 15 s.
* Boxes 2 and 4 toggle occupied / vacant from their load cell (above 3 g for
  1 s, below 2 g for 2 s, then re-zero). Boxes 1, 3 and 5 are always available.
* Refuses a route to an occupied box.
* Logs every run (route, result, time, weight before and after) and exports it
  as CSV for the 150-run test.
* Bring-up controls on the page: jog the belt, move each arm, tare and
  calibrate each load cell.

## Simulation mode

`KK_SIM_MODE` in `main/config.h` is `0` now that the HX711 boards are wired.
With it set to `1` the load cells are faked in software: a route to box 2 or 4 drops a 40 g part
when the belt has run far enough, and the page has "Place part" / "Remove
part" buttons. The stepper and servo code still runs and drives the real pins.

## Running without the motor

While the stepper and driver are not wired, set `KK_MOTOR_CONNECTED 0` in
`main/config.h`. The motor tests are skipped and the
dashboard refuses routes and jogs, but both load cells read live and the three
arms move from the Bring-up panel. Set it to `1` once the motor is wired.

## Build and flash

Open the `firmware/Kinetic_Kitchen` folder in VS Code with the ESP-IDF
extension and use its Build / Flash / Monitor buttons, or from the ESP-IDF
terminal:

```bash
idf.py build
```

```bash
idf.py -p COM5 flash monitor
```

Replace `COM5` with the board's port. The monitor prints the dashboard address
after boot.

If flashing stops at "Connecting...", put the board in download mode by hand:
hold BOOT (IO0), press and release EN, release BOOT, then flash again.

The project targets the classic ESP32 (`sdkconfig.defaults`). After changing
the target or the defaults, delete `sdkconfig` (or run `idf.py set-target
esp32`) so the new defaults are picked up.

The default `build/` folder inside the project works (it is already in
`.gitignore`). Because the project lives in OneDrive, the build output syncs
too; if that becomes a nuisance, point the ESP-IDF extension setting "Build
directory path" (`idf.buildPath`) at a short folder outside OneDrive such as
`C:\Users\nanot\kk-build`. Keep any build path short: ESP-IDF's mbedtls
sources overflow the Windows 260-character path limit in deep folders.

## Wi-Fi

In `main/config.h`:

* `KK_WIFI_STA_SSID` / `KK_WIFI_STA_PASS`: the 2.4 GHz hotspot from the
  laptop. If set, the ESP32 joins it first and prints its IP on the monitor.
* If the SSID is empty, or joining fails within 15 s, the ESP32 starts its own
  hotspot `KineticKitchen` (password `kinetic123`) and the page is at
  `http://192.168.4.1`.

## Wiring

All pins are in `main/config.h` and can be changed there.

| Signal | GPIO | Note |
| :--- | :--- | :--- |
| A4988 STEP | 25 | |
| A4988 DIR | 32 | flip `KK_DIR_FORWARD_LEVEL` if the belt runs the wrong way |
| A4988 ENABLE | 33 | active low, released between routes |
| Servo box 2 (servo 1) | 23 | SG90 signal, own 5 V supply |
| Servo box 3 (servo 2) | 19 | SG90 signal, own 5 V supply |
| Servo box 4 (servo 3) | 18 | SG90 signal, own 5 V supply |
| HX711 box 2 (load cell 1) DT / SCK | 26 / 27 | 3.3 V supply |
| HX711 box 4 (load cell 2) DT / SCK | 13 / 14 | 3.3 V supply |
| Stop button | 4 | to GND, enable with `KK_ESTOP_ENABLED 1` |
| LED | 2 | on-board blue LED, slow blink idle, fast blink routing |

Board labels on the NodeMCU-32S read `IOxx`; `IO25` is GPIO 25 and so on.
Leave free: 0, 2, 5, 12 and 15 (they decide how the chip boots), 6 to 11 (flash)
and 1 / 3 (USB serial).

### A4988

* **Microstepping** is set by MS1, MS2 and MS3 on the driver, not by the
  firmware. Unconnected means full step, which is what `KK_MICROSTEPS 1`
  assumes. Tying all three to 3.3 V or 5 V gives 1/16 step: quieter and
  smoother; set `KK_MICROSTEPS 16` to match.
* **Current limit** = VREF / (8 x Rsense). Read the sense resistors next to the
  chip: `R100` (0.1 Ohm) gives 1.65 V / 0.8 = 2.06 A, `R068` gives 3.0 A,
  `R050` gives 4.1 A. An A4988 manages about 1 A without a heatsink, so for a
  first test turn VREF down to about 0.8 V with R100 resistors (1 A) and fit a
  heatsink before raising it.
* **SLEEP and RESET** must be tied together (or both to 3.3 V), or the driver
  stays off.
* Put a 100 uF capacitor across VMOT and GND, right at the driver.

Power: 12 V to the A4988 VMOT, one 5 V buck converter for the servos, a
separate one for the ESP32 5V pin, 3.3 V from the ESP32 to the HX711 boards and
the A4988 VDD. Common ground everywhere. Keep a 100 to 470 uF capacitor across
the ESP32's 3V3 and GND, and another across the servo 5 V rail.

Do not power the board from the buck converter and USB at the same time:
unplug the buck's 5 V wire while the USB cable is connected for flashing.

## Things to measure before the Expo

Search `PLACEHOLDER` in `main/config.h`.

1. Belt positions of the three starts and five boxes (`KK_POS_STARTS`,
   `KK_POS_BOXES`), in mm, with 0 at box 2.
2. Microstep setting on the A4988 (`KK_MICROSTEPS`) and the belt direction.
   Jog +50 mm from the page and check the belt moves toward box 5 by 50 mm.
3. Servo rest and out angles for each arm.
4. Load cell scale: tare the empty box, put a known mass in it, type the grams
   on the page and press Calibrate. The value is saved to flash.
5. Occupied / vacant thresholds (`KK_OCCUPIED_G`, `KK_VACANT_G`) from real
   readings.

## Arm rest position at power-up

An SG90 has no memory, but it always goes to the angle it is told. The rest and
out angle of each arm are stored in the ESP32's flash. At every boot the firmware
sends each arm to its saved rest angle before anything else moves, so the arms
start flush with the conveyor. Set the angles once from the dashboard
(Bring-up and testing, Arm angles, Save). `KK_ARM_REST_DEG` and `KK_ARM_OUT_DEG`
in `config.h` are only the first-boot defaults.

## Load cell drift

Re-zeroing on a timer would erase the weight of a part that is in the box, so
the firmware separates slow creep from real changes instead:

* **Empty box.** While the reading stays within `KK_ZERO_TRACK_G` of zero, the
  zero point follows it continuously. This does what a tare every few minutes
  would do, without a jump.
* **Occupied box.** The part weight is latched when the part lands. Changes
  slower than `KK_DRIFT_RATE_G_PER_S` are treated as creep and removed, so the
  displayed weight stays at the part weight. The box goes vacant only when the
  weight drops quickly below `KK_REMOVED_FRACTION` of the part, and it is then
  re-zeroed.

## HTTP API

| Method | Path | Purpose |
| :--- | :--- | :--- |
| GET | `/` | dashboard |
| GET | `/api/status` | live state as JSON, polled by the page |
| POST | `/api/route?start=1&box=3` | run a route |
| POST | `/api/stop` | stop the belt and put the arm back |
| POST | `/api/tare?box=2` | re-zero a load cell |
| POST | `/api/calibrate?box=2&grams=100` | set the scale from a known mass |
| POST | `/api/box/clear?box=2` | clear a "needs check" flag |
| POST | `/api/jog?mm=50&dir=1` | move the belt (dir 1 toward box 5, -1 toward box 1) |
| POST | `/api/arm?box=3&out=1` | move an arm |
| POST | `/api/armcfg?box=3&rest=5&out=50` | set and save an arm's rest and out angles |
| POST | `/api/motor?on=0` | release the stepper |
| GET | `/api/log?n=20` | last n runs as JSON |
| GET | `/api/log.csv` | all runs as CSV |
| POST | `/api/log/clear` | clear the run log |
| POST | `/api/sim?box=2&grams=40` | simulation only: fake a part in a box |

The B.L.A.Z.E. backend can start a route with the same `POST /api/route` call
once the bed-clear signal exists (improvement B1).

## Layout

```
firmware/Kinetic_Kitchen/
  CMakeLists.txt         ESP-IDF project
  sdkconfig.defaults     target (ESP32), flash size, partition table, stack sizes
  .vscode/               ESP-IDF extension settings (generated by VS Code)
  main/
    config.h             pins, geometry, thresholds, Wi-Fi, simulation flag
    main.c               boot, heartbeat LED
    stepper.c/.h         step/dir driver (A4988) with a hardware-timer ramp
    tmc_uart.c/.h        TMC2209 UART setup (only with a TMC2209)
    driver_check.c/.h    TMC2209 bench check (only with a TMC2209)
    servo_arm.c/.h       three SG90 arms on LEDC
    hx711.c/.h           load cell amplifier (bit-banged) or simulation
    boxes.c/.h           occupied / vacant state machine, tare, calibration
    router.c/.h          route sequencer, stop, jog, e-stop
    runlog.c/.h          ring buffer of runs
    wifi_mgr.c/.h        laptop hotspot first, own hotspot as fallback (keeps retrying)
    wifi_secrets.h       hotspot name and password, git-ignored (create it yourself)
    web_server.c/.h      dashboard and JSON API
    web/index.html       the dashboard (embedded in the firmware)
```
