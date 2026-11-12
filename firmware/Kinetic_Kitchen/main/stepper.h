#pragma once
#include <stdbool.h>

// Belt drive: NEMA 17 through a TMC2209 in step/dir mode.
// Step pulses come from a hardware timer ISR with a trapezoid speed ramp.

void  stepper_init(void);
void  stepper_enable(bool on);          // TMC2209 EN pin (active low)
bool  stepper_enabled(void);
// Start a move. dir = +1 toward box 5, -1 toward box 1. Returns false if busy.
bool  stepper_move_mm(float mm, int dir);
void  stepper_stop(void);               // decelerate to a stop
void  stepper_abort(void);              // stop immediately (e-stop)
bool  stepper_is_moving(void);
float stepper_mm_moved(void);           // distance covered by the current / last move
int   stepper_dir(void);
float stepper_steps_per_mm(void);
