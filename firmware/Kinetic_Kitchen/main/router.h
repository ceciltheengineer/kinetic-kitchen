#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "runlog.h"

// Route sequencer (report section 10.3):
//   choose -> set the arm -> move the belt -> stop on weight or distance -> arm back.

typedef enum {
    RS_IDLE = 0,
    RS_ARM_OUT,
    RS_RUNNING,
    RS_SETTLING,
    RS_ARM_IN,
    RS_DONE,
    RS_ERROR,
    RS_STOPPED,
    RS_ESTOP,
} route_state_t;

typedef struct {
    route_state_t state;
    int      start;
    int      box;
    uint32_t run_id;
    uint32_t elapsed_ms;
    float    expected_mm;   // belt travel planned for this route
    bool     estop;
    char     msg[64];
} router_status_t;

void  router_init(void);
bool  router_busy(void);
void  router_get_status(router_status_t *out);

// Commands. They return false if the sequencer is busy with a route.
bool  router_route(int start, int box);
void  router_stop(void);
bool  router_jog(float mm, int dir);
bool  router_spin(int dir);              // motor on at belt speed until Stop (10 min cap)
bool  router_arm(int box, bool out);
bool  router_motor_off(void);

float router_start_pos(int start);   // mm, start 1..3
float router_box_pos(int box);       // mm, box 1..5
const char *router_state_str(route_state_t s);
