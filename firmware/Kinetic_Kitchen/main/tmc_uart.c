#include "tmc_uart.h"
#include "config.h"

#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "tmc";

#define PORT ((uart_port_t)KK_TMC_UART_PORT)
#define SYNC 0x05

static bool s_ready;
static uint8_t s_addr = KK_TMC_ADDR;   // set by MS1/MS2 on the driver

// CRC8 from the TMC2209 datasheet (polynomial x^8 + x^2 + x + 1, LSB first).
static uint8_t crc8(const uint8_t *d, int len)
{
    uint8_t crc = 0;
    for (int i = 0; i < len; i++) {
        uint8_t b = d[i];
        for (int j = 0; j < 8; j++) {
            if ((crc >> 7) ^ (b & 1)) {
                crc = (uint8_t)((crc << 1) ^ 0x07);
            } else {
                crc = (uint8_t)(crc << 1);
            }
            b >>= 1;
        }
    }
    return crc;
}

void tmc_uart_init(void)
{
    if (s_ready) {
        return;
    }
    uart_config_t c = {
        .baud_rate = KK_TMC_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(PORT, 256, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(PORT, &c));
    ESP_ERROR_CHECK(uart_set_pin(PORT, KK_PIN_UART_ESP_TX, KK_PIN_UART_ESP_RX,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    gpio_pullup_en(KK_PIN_UART_ESP_RX);   // defined idle level if the driver is unpowered
    s_ready = true;
}

static tmc_result_t xfer(const uint8_t *tx, int txlen, uint8_t *rx, int rxlen)
{
    uint8_t echo[8];
    uart_flush_input(PORT);
    uart_write_bytes(PORT, tx, txlen);
    uart_wait_tx_done(PORT, pdMS_TO_TICKS(20));
    int n = uart_read_bytes(PORT, echo, txlen, pdMS_TO_TICKS(20));
    if (n <= 0) {
        return TMC_NO_ECHO;
    }
    if (n != txlen || memcmp(echo, tx, txlen) != 0) {
        return TMC_BAD_ECHO;
    }
    if (rxlen > 0) {
        n = uart_read_bytes(PORT, rx, rxlen, pdMS_TO_TICKS(20));
        if (n <= 0) {
            return TMC_NO_REPLY;
        }
        if (n != rxlen) {
            return TMC_BAD_REPLY;
        }
    }
    vTaskDelay(pdMS_TO_TICKS(2));   // idle gap before the next datagram
    return TMC_OK;
}

static tmc_result_t read_once(uint8_t reg, uint32_t *val)
{
    uint8_t req[4] = { SYNC, s_addr, (uint8_t)(reg & 0x7F), 0 };
    req[3] = crc8(req, 3);
    uint8_t rep[8];
    tmc_result_t r = xfer(req, 4, rep, 8);
    if (r != TMC_OK) {
        return r;
    }
    if (rep[0] != SYNC || rep[1] != 0xFF || rep[2] != (reg & 0x7F) || rep[7] != crc8(rep, 7)) {
        return TMC_BAD_REPLY;
    }
    *val = ((uint32_t)rep[3] << 24) | ((uint32_t)rep[4] << 16) | ((uint32_t)rep[5] << 8) | rep[6];
    return TMC_OK;
}

tmc_result_t tmc_read(uint8_t reg, uint32_t *val)
{
    tmc_result_t r = read_once(reg, val);
    if (r != TMC_OK) {
        r = read_once(reg, val);   // one retry
    }
    return r;
}

tmc_result_t tmc_write(uint8_t reg, uint32_t v)
{
    uint8_t d[8] = { SYNC, s_addr, (uint8_t)(reg | 0x80),
                     (uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v, 0 };
    d[7] = crc8(d, 7);
    return xfer(d, 8, NULL, 0);
}

int tmc_find_address(void)
{
    uint8_t saved = s_addr;
    for (uint8_t a = 0; a < 4; a++) {
        uint32_t v;
        s_addr = a;
        if (tmc_read(TMC_REG_IOIN, &v) == TMC_OK) {
            return a;
        }
    }
    s_addr = saved;
    return -1;
}

const char *tmc_result_str(tmc_result_t r)
{
    switch (r) {
    case TMC_OK:        return "ok";
    case TMC_NO_ECHO:   return "no echo";
    case TMC_BAD_ECHO:  return "garbled echo";
    case TMC_NO_REPLY:  return "no reply";
    case TMC_BAD_REPLY: return "garbled reply";
    }
    return "?";
}

tmc_result_t tmc_set_current(int run_ma, int hold_pct, float *run_ma_actual)
{
    uint32_t ifcnt0, ifcnt1, gconf, chop;
    tmc_result_t r;
    if ((r = tmc_read(TMC_REG_IFCNT, &ifcnt0)) != TMC_OK) return r;
    if ((r = tmc_read(TMC_REG_GCONF, &gconf)) != TMC_OK) return r;
    if ((r = tmc_read(TMC_REG_CHOPCONF, &chop)) != TMC_OK) return r;

    // Full-scale RMS current at CS = 31, from the TMC2209 datasheet.
    float vfs = (chop & (1u << 17)) ? 0.180f : 0.325f;   // vsense bit
    float ifs_ma = vfs / (KK_TMC_RSENSE_OHM + 0.02f) / sqrtf(2.0f) * 1000.0f;
    int irun = (int)lroundf(32.0f * (float)run_ma / ifs_ma) - 1;
    if (irun < 0) irun = 0;
    if (irun > 31) irun = 31;
    int ihold = (int)lroundf((float)(irun + 1) * (float)hold_pct / 100.0f) - 1;
    if (ihold < 0) ihold = 0;
    if (ihold > 31) ihold = 31;

    gconf &= ~(1u << 0);   // I_scale_analog = 0: internal reference, knob ignored
    gconf |= (1u << 6);    // pdn_disable = 1: PDN pin is now only the UART
    if ((r = tmc_write(TMC_REG_GCONF, gconf)) != TMC_OK) return r;
    uint32_t ihr = (uint32_t)ihold | ((uint32_t)irun << 8) | (4u << 16);
    if ((r = tmc_write(TMC_REG_IHOLD_IRUN, ihr)) != TMC_OK) return r;

    // The driver counts every write it accepted.
    if ((r = tmc_read(TMC_REG_IFCNT, &ifcnt1)) != TMC_OK) return r;
    if (((ifcnt1 - ifcnt0) & 0xFF) != 2) {
        ESP_LOGE(TAG, "driver accepted %lu of 2 writes", (unsigned long)((ifcnt1 - ifcnt0) & 0xFF));
        return TMC_BAD_REPLY;
    }
    uint32_t back;
    if ((r = tmc_read(TMC_REG_GCONF, &back)) != TMC_OK) return r;
    if ((back & 0x41) != 0x40) {
        ESP_LOGE(TAG, "GCONF read back 0x%08lx, write did not stick", (unsigned long)back);
        return TMC_BAD_REPLY;
    }
    float actual = (float)(irun + 1) / 32.0f * ifs_ma;
    if (run_ma_actual) {
        *run_ma_actual = actual;
    }
    ESP_LOGI(TAG, "current set: run %.0f mA (IRUN %d), hold %d%% (IHOLD %d). VREF knob ignored",
             actual, irun, hold_pct, ihold);
    return TMC_OK;
}

bool tmc_configure(void)
{
    tmc_uart_init();
    tmc_result_t r = tmc_set_current(KK_TMC_RUN_MA, KK_TMC_HOLD_PCT, NULL);
    if (r != TMC_OK) {
        ESP_LOGW(TAG, "driver did not answer over UART (%s). Current comes from the VREF knob", tmc_result_str(r));
        return false;
    }
    return true;
}
