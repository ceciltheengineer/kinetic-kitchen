#pragma once

// Bench check for the TMC2209 over UART: link, wiring of EN/STEP/DIR,
// current setting, shorts, temperature and a slow move. Never returns.
__attribute__((noreturn)) void driver_check_run(void);
