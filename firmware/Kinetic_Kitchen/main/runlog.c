#include "runlog.h"
#include "config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static run_entry_t s_log[KK_LOG_CAPACITY];
static int s_head;    // next write slot
static int s_count;
static SemaphoreHandle_t s_lock;

void runlog_init(void)
{
    s_lock = xSemaphoreCreateMutex();
}

void runlog_add(const run_entry_t *e)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_log[s_head] = *e;
    s_head = (s_head + 1) % KK_LOG_CAPACITY;
    if (s_count < KK_LOG_CAPACITY) {
        s_count++;
    }
    xSemaphoreGive(s_lock);
}

int runlog_count(void)
{
    return s_count;
}

bool runlog_get(int idx, run_entry_t *out)
{
    bool ok = false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (idx >= 0 && idx < s_count) {
        int oldest = (s_head - s_count + KK_LOG_CAPACITY) % KK_LOG_CAPACITY;
        *out = s_log[(oldest + idx) % KK_LOG_CAPACITY];
        ok = true;
    }
    xSemaphoreGive(s_lock);
    return ok;
}

void runlog_clear(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_head = 0;
    s_count = 0;
    xSemaphoreGive(s_lock);
}

const char *runlog_result_str(run_result_t r)
{
    switch (r) {
    case RES_OK:            return "ok";
    case RES_TIMEOUT:       return "timeout";
    case RES_NOT_CONFIRMED: return "not_confirmed";
    case RES_STOPPED:       return "stopped";
    case RES_ESTOP:         return "estop";
    case RES_REJECTED:      return "rejected";
    }
    return "?";
}
