#pragma once
#include <stdint.h>
#include <stdbool.h>

// One line per route for the 150-run test (improvement A4).
// Kept in RAM as a ring buffer; exported as CSV by the web server.

typedef enum {
    RES_OK = 0,
    RES_TIMEOUT,        // no confirmation within KK_ROUTE_TIMEOUT_MS
    RES_NOT_CONFIRMED,  // belt ran but the weight never settled above threshold
    RES_STOPPED,        // stop button on the page
    RES_ESTOP,          // physical stop button
    RES_REJECTED,       // box not available / bad request
} run_result_t;

typedef struct {
    uint32_t     id;
    int64_t      t_start_ms;   // uptime when the route started
    int          start;
    int          box;
    run_result_t result;
    uint32_t     duration_ms;
    float        g_before;
    float        g_after;
    float        mm_moved;
} run_entry_t;

void        runlog_init(void);
void        runlog_add(const run_entry_t *e);
int         runlog_count(void);
bool        runlog_get(int idx, run_entry_t *out);   // idx 0 = oldest
void        runlog_clear(void);
const char *runlog_result_str(run_result_t r);
