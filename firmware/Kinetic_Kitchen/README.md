# Kinetic Kitchen firmware

ESP32 WROOM-32 firmware and live dashboard for the sorting conveyor prototype
(3 start positions, 5 boxes, reversible belt, 3 servo arms, 2 load cells).
Built with ESP-IDF v6.1 in VS Code.

## What it does

* Serves the dashboard at `http://192.168.4.1` (hotspot) or at the IP it gets
  from the laptop hotspot.
* Runs a route: pick a start position and a box, the arm for that box swings
  out, the belt runs toward the box, the belt stops when the load cell sees the
  part land (boxes 2 and 4) or after the planned distance (boxes 1, 3, 5), the
  arm comes back and the motor is released. A route is aborted after 10 s.
* Boxes 2 and 4 toggle occupied / vacant from their load cell (above 3 g for
  1 s, below 2 g for 2 s, then re-zero). Boxes 1, 3 and 5 are always available.
* Refuses a route to an occupied box.
* Logs every run (route, result, time, weight before and after) and exports it
  as CSV for the 150-run test.
* Bring-up controls on the page: jog the belt, move each arm, tare and
  calibrate each load cell.

## Simulation mode

The electronics are not wired yet, so `KK_SIM_MODE` in `main/config.h` is `1`.
The load cells are faked in software: a route to box 2 or 4 drops a 40 g part
when the belt has run far enough, and the page has "Place part" / "Remove
part" buttons. The stepper and servo code still runs and drives the real pins.
Set `KK_SIM_MODE` to `0` once the HX711 boards are connected.

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

## Wiring (placeholder pin map)

All pins are in `main/config.h` and can be changed there.

| Signal | GPIO | Note |
| :--- | :--- | :--- |
| TMC2209 STEP | 25 | |
| TMC2209 DIR | 26 | flip `KK_DIR_FORWARD_LEVEL` if the belt runs the wrong way |
| TMC2209 EN | 27 | active low, released between routes |
| Servo box 2 | 18 | SG90 signal |
| Servo box 3 | 19 | SG90 signal |
| Servo box 4 | 23 | SG90 signal |
| HX711 box 2 DOUT / SCK | 32 / 33 | |
| HX711 box 4 DOUT / SCK | 34 / 14 | 34 is input only, fine for DOUT |
| Stop button | 4 | to GND, enable with `KK_ESTOP_ENABLED 1` |
| LED | 2 | on-board, slow blink idle, fast blink routing |

Power per the report: 12 V to the TMC2209 VM, 5 V from the buck converter to
the servos, the HX711 boards and the ESP32 5V pin. Common ground everywhere.

## Things to measure before the Expo

Search `PLACEHOLDER` in `main/config.h`.

1. Belt positions of the three starts and five boxes (`KK_POS_STARTS`,
   `KK_POS_BOXES`), in mm, with 0 at box 2.
2. Microstep setting on the TMC2209 (`KK_MICROSTEPS`) and the belt direction.
   Jog +50 mm from the page and check the belt moves toward box 5 by 50 mm.
3. Servo rest and out angles for each arm.
4. Load cell scale: tare the empty box, put a known mass in it, type the grams
   on the page and press Calibrate. The value is saved to flash.
5. Occupied / vacant thresholds (`KK_OCCUPIED_G`, `KK_VACANT_G`) from real
   readings.

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
  sdkconfig.defaults     target, partition table, task stack sizes
  .vscode/               ESP-IDF extension settings (generated by VS Code)
  main/
    config.h             pins, geometry, thresholds, Wi-Fi, simulation flag
    main.c               boot, heartbeat LED
    stepper.c/.h         TMC2209 step/dir with a hardware-timer ramp
    servo_arm.c/.h       three SG90 arms on LEDC
    hx711.c/.h           load cell amplifier (bit-banged) or simulation
    boxes.c/.h           occupied / vacant state machine, tare, calibration
    router.c/.h          route sequencer, stop, jog, e-stop
    runlog.c/.h          ring buffer of runs
    wifi_mgr.c/.h        laptop hotspot first, own hotspot as fallback
    web_server.c/.h      dashboard and JSON API
    web/index.html       the dashboard (embedded in the firmware)
```
