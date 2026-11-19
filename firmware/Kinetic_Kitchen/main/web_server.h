#pragma once

// Serves the dashboard and the JSON API.
//
//   GET  /                      dashboard page
//   GET  /api/status            live state (polled by the page)
//   POST /api/route?start=1&box=3
//   POST /api/stop
//   POST /api/tare?box=2
//   POST /api/calibrate?box=2&grams=100
//   POST /api/box/clear?box=2   clear a "needs check" flag
//   POST /api/jog?mm=50&dir=1   bring-up: move the belt
//   POST /api/arm?box=3&out=1   bring-up: move an arm
//   POST /api/armcfg?box=3&rest=5&out=50   set and save an arm's rest / out angles
//   POST /api/motor?on=0        bring-up: release the stepper
//   GET  /api/log?n=20          last n runs as JSON (newest first)
//   GET  /api/log.csv           every logged run as CSV
//   POST /api/log/clear
//   POST /api/sim?box=2&grams=40   simulation only: fake a part in a box

void web_server_start(void);
