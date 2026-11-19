# Kinetic Kitchen

Automated sorting and handoff for 3D print farms. A conveyor in front of a row of
printers sends each finished part to its own box, and load cells show which boxes
are taken. Built for the FrED Factory Lab, Tecnológico de Monterrey, Fall 2026
(Expo Ingenierías, December 1, 2026).

This repository holds the project report and the prototype firmware.

| Path | What it is |
| :--- | :--- |
| [KineticKitchen_Report.md](KineticKitchen_Report.md) | Full project report: problem, design, calculations, test plan |
| [figures/](figures) | Prototype photo, dimensions and demo layout used in the report |
| [firmware/Kinetic_Kitchen/](firmware/Kinetic_Kitchen/README.md) | ESP32 firmware (ESP-IDF v6.1) and the live control dashboard |

## The prototype

* 65 cm conveyor with 3 start positions and 5 collection boxes.
* NEMA 17 stepper through an A4988 drives a reversible belt. Boxes 1 and 5 are at the
  ends of the belt. Boxes 2, 3 and 4 are at the side, each with a 45° servo arm.
* Boxes 2 and 4 sit on load cells (HX711) and toggle occupied / available from weight.
  Boxes 1, 3 and 5 are always available.
* The ESP32 serves a web dashboard to choose a start position and a box, run the route,
  stop it, and watch box status live. Every run is logged and can be exported as CSV.

![Demo layout](figures/demo_layout.png)

## Getting started with the firmware

See [firmware/Kinetic_Kitchen/README.md](firmware/Kinetic_Kitchen/README.md) for the pin
map, build and flash steps, Wi-Fi options, simulation mode and the HTTP API.

## Status

The firmware builds cleanly and the dashboard has been tested against a mock of the API.
It has not yet been run on the real hardware: pins, belt positions, microsteps, servo angles
and load cell calibration are placeholders to be measured. Open items in the report are
marked `[TODO]`.
