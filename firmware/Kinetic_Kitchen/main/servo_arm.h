#pragma once
#include <stdbool.h>

// Three SG90 arms on boxes 2, 3 and 4, driven by LEDC at 50 Hz.

void servo_arm_init(void);
bool arm_set(int box, bool out);     // box 2..4. true = swing out to 45 deg
bool arm_is_out(int box);
void arms_all_in(void);
bool box_has_arm(int box);
