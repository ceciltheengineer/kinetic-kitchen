#pragma once
#include <stdbool.h>

// Three SG90 arms on boxes 2, 3 and 4, driven by LEDC at 50 Hz.

void servo_arm_init(void);
bool arm_set(int box, bool out);     // box 2..4. true = swing out to the forward out angle
bool arm_set_out_dir(int box, int dir);   // swing out to face a part on a belt moving in dir (+1 / -1)
bool arm_is_out(int box);
bool arm_zero(int box, float servo_deg, bool save);   // find / store the arm's 0 point
float arm_zero_get(int box);                          // servo angle that is arm angle 0
float arm_angle_now(int box);        // angle last sent to the arm, degrees
void arms_all_in(void);
bool box_has_arm(int box);

// Rest (flush with the conveyor) and out angles for one arm, in degrees.
// arm_set_angles saves them to flash and moves the arm to its current
// position at the new angle. They are loaded again at every boot.
bool arm_set_angles(int box, float rest_deg, float out_deg);
bool arm_get_angles(int box, float *rest_deg, float *out_deg);

// Bench test for one servo: drive it straight to deg (0..180) without touching
// the saved angles, then read the signal pin back and report the pulse width
// actually present on it in microseconds (0 = no pulse seen on the pin).
bool arm_test_angle(int box, float deg, int *pulse_us);
