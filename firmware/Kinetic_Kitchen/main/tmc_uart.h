#pragma once
#include <stdbool.h>
#include <stdint.h>

// TMC2209 single-wire UART on the driver's PDN_UART pin.
// Wiring: ESP TX -> 1 kOhm -> driver UART pin, ESP RX -> same driver pin.
// The ESP hears its own bytes (the echo) before the driver's reply.

#define TMC_REG_GCONF      0x00
#define TMC_REG_GSTAT      0x01
#define TMC_REG_IFCNT      0x02
#define TMC_REG_IOIN       0x06
#define TMC_REG_IHOLD_IRUN 0x10
#define TMC_REG_CHOPCONF   0x6C
#define TMC_REG_DRV_STATUS 0x6F

typedef enum {
    TMC_OK = 0,
    TMC_NO_ECHO,     // ESP did not hear its own bytes: TX and RX not joined
    TMC_BAD_ECHO,    // heard something else: noise or a wiring fault
    TMC_NO_REPLY,    // echo fine but the driver stayed silent
    TMC_BAD_REPLY,   // reply incomplete or failed its checksum
} tmc_result_t;

void         tmc_uart_init(void);
tmc_result_t tmc_read(uint8_t reg, uint32_t *val);
tmc_result_t tmc_write(uint8_t reg, uint32_t val);
const char  *tmc_result_str(tmc_result_t r);

// Try driver addresses 0-3 (set by MS1/MS2). Keeps the one that answers and
// returns it, or returns -1 if none answered.
int          tmc_find_address(void);

// Switch the driver to its internal current reference (the VREF knob is then
// ignored) and set run and hold current. Writes are confirmed through the
// driver's write counter. run_ma_actual gets the nearest current it can do.
tmc_result_t tmc_set_current(int run_ma, int hold_pct, float *run_ma_actual);

// tmc_uart_init + tmc_set_current with the values from config.h, logging the
// result. Returns false if the driver did not answer (the knob then rules).
bool tmc_configure(void);
