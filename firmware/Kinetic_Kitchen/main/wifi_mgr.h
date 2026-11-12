#pragma once

// Joins KK_WIFI_STA_SSID if one is configured, otherwise (or on failure)
// starts the KineticKitchen hotspot at 192.168.4.1.

typedef struct {
    char mode[8];    // "sta" or "ap"
    char ssid[33];
    char ip[16];
} wifi_info_t;

void wifi_mgr_start(void);                 // blocks until connected or the hotspot is up
void wifi_mgr_get_info(wifi_info_t *out);
