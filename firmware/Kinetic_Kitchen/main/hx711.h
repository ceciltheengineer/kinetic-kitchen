#pragma once
#include <stdbool.h>
#include <stdint.h>

// HX711 load cell amplifiers (bit-banged, channel A, gain 128).
// With KK_SIM_MODE the readings are generated in software.

void hx711_init(void);
bool hx711_read_raw(int cell, int32_t *raw);   // cell 0..KK_NUM_CELLS-1, blocks up to ~200 ms

#if KK_SIM_MODE
void  hx711_sim_set_grams(int cell, float grams);
float hx711_sim_get_grams(int cell);
#endif
