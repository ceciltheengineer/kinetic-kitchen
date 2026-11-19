#include "wifi_mgr.h"
#include "config.h"

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "wifi";

#define BIT_CONNECTED BIT0
#define BIT_FAILED    BIT1
#define STA_RETRIES   5
#define STA_RETRY_US  (15 * 1000 * 1000)   // while on the fallback hotspot, try the laptop again this often

static EventGroupHandle_t s_ev;
static esp_netif_t *s_sta_netif;
static esp_netif_t *s_ap_netif;
static wifi_info_t s_info;
static int  s_retries;
static bool s_ap_mode;
static bool s_was_connected;
static esp_timer_handle_t s_retry_timer;

static void retry_cb(void *arg)
{
    esp_wifi_connect();
}

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_ap_mode) {
            // Running our own hotspot as a fallback: keep looking for the
            // laptop hotspot in the background, so turning it on later (or
            // powering the board before the laptop) still ends up joined.
            if (s_retry_timer && !esp_timer_is_active(s_retry_timer)) {
                esp_timer_start_once(s_retry_timer, STA_RETRY_US);
            }
            return;
        }
        if (s_was_connected) {
            // The hotspot went away after we had joined it: keep trying.
            ESP_LOGW(TAG, "disconnected, reconnecting");
            strlcpy(s_info.ip, "0.0.0.0", sizeof s_info.ip);
            esp_wifi_connect();
        } else if (s_retries < STA_RETRIES) {
            s_retries++;
            ESP_LOGI(TAG, "retry %d/%d", s_retries, STA_RETRIES);
            esp_wifi_connect();
        } else {
            xEventGroupSetBits(s_ev, BIT_FAILED);
        }
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STACONNECTED) {
        ESP_LOGI(TAG, "a device joined the hotspot");
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        snprintf(s_info.ip, sizeof s_info.ip, IPSTR, IP2STR(&e->ip_info.ip));
        s_retries = 0;
        s_was_connected = true;
        if (s_ap_mode) {
            // Found the laptop hotspot after falling back: drop our own.
            s_ap_mode = false;
            esp_wifi_set_mode(WIFI_MODE_STA);
            strlcpy(s_info.mode, "sta", sizeof s_info.mode);
            strlcpy(s_info.ssid, KK_WIFI_STA_SSID, sizeof s_info.ssid);
        }
        xEventGroupSetBits(s_ev, BIT_CONNECTED);
        ESP_LOGI(TAG, "joined %s, dashboard at http://%s", KK_WIFI_STA_SSID, s_info.ip);
    }
}

// keep_sta: also keep retrying the laptop hotspot in the background.
static void start_ap(bool keep_sta)
{
    s_ap_mode = true;
    wifi_config_t wc = { 0 };
    strlcpy((char *)wc.ap.ssid, KK_WIFI_AP_SSID, sizeof wc.ap.ssid);
    wc.ap.ssid_len = strlen(KK_WIFI_AP_SSID);
    strlcpy((char *)wc.ap.password, KK_WIFI_AP_PASS, sizeof wc.ap.password);
    wc.ap.channel = KK_WIFI_AP_CHANNEL;
    wc.ap.max_connection = 4;
    wc.ap.authmode = (strlen(KK_WIFI_AP_PASS) >= 8) ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(keep_sta ? WIFI_MODE_APSTA : WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());   // in APSTA this also starts the STA retries

    esp_netif_ip_info_t ip;
    esp_netif_get_ip_info(s_ap_netif, &ip);
    snprintf(s_info.ip, sizeof s_info.ip, IPSTR, IP2STR(&ip.ip));
    strlcpy(s_info.mode, "ap", sizeof s_info.mode);
    strlcpy(s_info.ssid, KK_WIFI_AP_SSID, sizeof s_info.ssid);
    ESP_LOGI(TAG, "hotspot \"%s\" up, dashboard at http://%s", KK_WIFI_AP_SSID, s_info.ip);
    if (keep_sta) {
        ESP_LOGI(TAG, "still looking for \"%s\" every %d s", KK_WIFI_STA_SSID, STA_RETRY_US / 1000000);
    }
}

void wifi_mgr_start(void)
{
    s_ev = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_sta_netif = esp_netif_create_default_wifi_sta();
    s_ap_netif  = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, NULL));
    strlcpy(s_info.ip, "0.0.0.0", sizeof s_info.ip);
    const esp_timer_create_args_t ta = { .callback = retry_cb, .name = "sta_retry" };
    ESP_ERROR_CHECK(esp_timer_create(&ta, &s_retry_timer));

    if (strlen(KK_WIFI_STA_SSID) > 0) {
        wifi_config_t wc = { 0 };
        strlcpy((char *)wc.sta.ssid, KK_WIFI_STA_SSID, sizeof wc.sta.ssid);
        strlcpy((char *)wc.sta.password, KK_WIFI_STA_PASS, sizeof wc.sta.password);
        wc.sta.threshold.authmode = (strlen(KK_WIFI_STA_PASS) > 0) ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
        ESP_ERROR_CHECK(esp_wifi_start());
        esp_wifi_set_ps(WIFI_PS_NONE);   // snappier dashboard polling
        ESP_LOGI(TAG, "joining \"%s\"...", KK_WIFI_STA_SSID);

        EventBits_t bits = xEventGroupWaitBits(s_ev, BIT_CONNECTED | BIT_FAILED, pdFALSE, pdFALSE,
                                               pdMS_TO_TICKS(KK_WIFI_STA_TIMEOUT_MS));
        if (bits & BIT_CONNECTED) {
            strlcpy(s_info.mode, "sta", sizeof s_info.mode);
            strlcpy(s_info.ssid, KK_WIFI_STA_SSID, sizeof s_info.ssid);
            return;
        }
        ESP_LOGW(TAG, "could not join \"%s\", starting own hotspot meanwhile", KK_WIFI_STA_SSID);
        s_ap_mode = true;   // disconnect handler now schedules slow retries
        esp_wifi_stop();
        start_ap(true);
        return;
    }
    start_ap(false);
}

void wifi_mgr_get_info(wifi_info_t *out)
{
    *out = s_info;
}
