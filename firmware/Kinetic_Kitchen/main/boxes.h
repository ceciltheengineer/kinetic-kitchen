#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "config.h"

// Box occupancy. Boxes with a load cell toggle occupied/vacant from the
// weight reading (report 7.3). Boxes without one are always available.

typedef enum {
    BOX_FREE = 0,
    BOX_OCCUPIED,
    BOX_NEEDS_CHECK,   // a route to this box timed out; cleared by hand
} box_state_t;

typedef struct {
    int         id;         // 1..5
    bool        has_cell;
    int         cell;       // HX711 index, or -1
    box_state_t state;
    float       grams;
    int32_t     raw;
    int32_t     offset;
    float       scale;      // counts per gram
    bool        taring;
} box_info_t;

void        boxes_init(void);                        // starts the sampling task
void        boxes_get(box_info_t out[KK_NUM_BOXES]); // snapshot
box_state_t boxes_state(int box);
float       boxes_grams(int box);
bool        boxes_has_cell(int box);
int         boxes_cell_index(int box);               // -1 if none
bool        boxes_available(int box);                // free, or no cell
void        boxes_tare(int box);                     // re-zero (averages the next readings)
void        boxes_calibrate(int box, float known_g); // set scale from a known mass, saved to NVS
void        boxes_set_needs_check(int box);
void        boxes_clear_check(int box);
const char *boxes_state_str(box_state_t s);
