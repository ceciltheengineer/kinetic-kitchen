#include "web_server.h"
#include "config.h"
#include "router.h"
#include "boxes.h"
#include "runlog.h"
#include "stepper.h"
#include "servo_arm.h"
#include "wifi_mgr.h"
#include "hx711.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "web";

extern const char index_html_start[] asm("_binary_index_html_start");

static httpd_handle_t s_server;
static char s_buf[4096];   // handlers run one at a time in the httpd task

// ---- helpers ----
static bool q_get(httpd_req_t *req, const char *key, char *val, size_t n)
{
    char qs[160];
    if (httpd_req_get_url_query_str(req, qs, sizeof qs) != ESP_OK) {
        return false;
    }
    return httpd_query_key_value(qs, key, val, n) == ESP_OK;
}

static bool q_int(httpd_req_t *req, const char *key, int *out)
{
    char v[16];
    if (!q_get(req, key, v, sizeof v)) return false;
    *out = atoi(v);
    return true;
}

static bool q_float(httpd_req_t *req, const char *key, float *out)
{
    char v[16];
    if (!q_get(req, key, v, sizeof v)) return false;
    *out = strtof(v, NULL);
    return true;
}

static esp_err_t send_json(httpd_req_t *req, const char *status, const char *json)
{
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, json);
}

static esp_err_t reply_ok(httpd_req_t *req)
{
    return send_json(req, "200 OK", "{\"ok\":true}");
}

static esp_err_t reply_fail(httpd_req_t *req, const char *status, const char *msg)
{
    char b[128];
    snprintf(b, sizeof b, "{\"ok\":false,\"error\":\"%s\"}", msg);
    return send_json(req, status, b);
}

// ---- handlers ----
static esp_err_t h_index(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, index_html_start, strlen(index_html_start));
}

static esp_err_t h_status(httpd_req_t *req)
{
    router_status_t rs;
    router_get_status(&rs);
    box_info_t bx[KK_NUM_BOXES];
    boxes_get(bx);
    wifi_info_t wi;
    wifi_mgr_get_info(&wi);

    int n = 0;
    size_t cap = sizeof s_buf;
#define P(...) n += snprintf(s_buf + n, (n < (int)cap) ? cap - n : 0, __VA_ARGS__)

    P("{\"fw\":\"%s\",\"sim\":%s,\"motor\":%s,\"uptime_ms\":%lld,", KK_FW_VERSION,
      KK_SIM_MODE ? "true" : "false", KK_MOTOR_CONNECTED ? "true" : "false", (long long)(esp_timer_get_time() / 1000));
    P("\"wifi\":{\"mode\":\"%s\",\"ssid\":\"%s\",\"ip\":\"%s\"},", wi.mode, wi.ssid, wi.ip);

    P("\"geom\":{\"starts\":[");
    for (int i = 1; i <= KK_NUM_STARTS; i++) P("%s%.1f", i > 1 ? "," : "", router_start_pos(i));
    P("],\"boxes\":[");
    for (int i = 1; i <= KK_NUM_BOXES; i++) P("%s%.1f", i > 1 ? "," : "", router_box_pos(i));
    P("],\"speed\":%.0f,\"arm_cross\":%.0f,\"end_over\":%.0f},",
      KK_BELT_SPEED_MM_S, KK_ARM_CROSS_MM, KK_END_OVERSHOOT_MM);

    P("\"belt\":{\"moving\":%s,\"dir\":%d,\"mm\":%.1f,\"enabled\":%s},",
      stepper_is_moving() ? "true" : "false", stepper_dir(), stepper_mm_moved(),
      stepper_enabled() ? "true" : "false");
    P("\"arms\":[%s,%s,%s],", arm_is_out(2) ? "true" : "false",
      arm_is_out(3) ? "true" : "false", arm_is_out(4) ? "true" : "false");
    P("\"arm_deg\":[%.0f,%.0f,%.0f],", arm_angle_now(2), arm_angle_now(3), arm_angle_now(4));
    P("\"arm_zero\":[%.0f,%.0f,%.0f],", arm_zero_get(2), arm_zero_get(3), arm_zero_get(4));
    P("\"arm_cfg\":[");
    for (int b = 2; b <= 4; b++) {
        float rest = 0.0f, out = 0.0f;
        arm_get_angles(b, &rest, &out);
        P("%s[%.0f,%.0f]", b > 2 ? "," : "", rest, out);
    }
    P("],");
    P("\"estop\":%s,", rs.estop ? "true" : "false");

    P("\"route\":{\"state\":\"%s\",\"start\":%d,\"box\":%d,\"run_id\":%lu,\"elapsed_ms\":%lu,"
      "\"expected_mm\":%.0f,\"msg\":\"%s\"},",
      router_state_str(rs.state), rs.start, rs.box, (unsigned long)rs.run_id,
      (unsigned long)rs.elapsed_ms, rs.expected_mm, rs.msg);

    P("\"boxes\":[");
    for (int i = 0; i < KK_NUM_BOXES; i++) {
        P("%s{\"id\":%d,\"cell\":%s,\"state\":\"%s\",\"g\":%.1f,\"raw\":%ld,\"taring\":%s}",
          i ? "," : "", bx[i].id, bx[i].has_cell ? "true" : "false",
          boxes_state_str(bx[i].state), bx[i].grams, (long)bx[i].raw,
          bx[i].taring ? "true" : "false");
    }
    P("],\"log_count\":%d}", runlog_count());
#undef P
    if (n >= (int)cap) {
        return reply_fail(req, "500 Internal Server Error", "status too large");
    }
    return send_json(req, "200 OK", s_buf);
}

static esp_err_t h_route(httpd_req_t *req)
{
    if (!KK_MOTOR_CONNECTED) {
        return reply_fail(req, "409 Conflict", "motor not connected (KK_MOTOR_CONNECTED is 0)");
    }
    int start, box;
    if (!q_int(req, "start", &start) || !q_int(req, "box", &box)) {
        return reply_fail(req, "400 Bad Request", "need start and box");
    }
    if (start < 1 || start > KK_NUM_STARTS || box < 1 || box > KK_NUM_BOXES) {
        return reply_fail(req, "400 Bad Request", "start 1-3, box 1-5");
    }
    if (router_busy()) {
        return reply_fail(req, "409 Conflict", "a route is already running");
    }
    if (!boxes_available(box)) {
        char m[64];
        snprintf(m, sizeof m, "box %d is %s", box, boxes_state_str(boxes_state(box)));
        return reply_fail(req, "409 Conflict", m);
    }
    if (!router_route(start, box)) {
        return reply_fail(req, "409 Conflict", "could not queue route");
    }
    ESP_LOGI(TAG, "route start %d -> box %d", start, box);
    return reply_ok(req);
}

static esp_err_t h_stop(httpd_req_t *req)
{
    router_stop();
    return reply_ok(req);
}

static esp_err_t h_tare(httpd_req_t *req)
{
    int box;
    if (!q_int(req, "box", &box) || !boxes_has_cell(box)) {
        return reply_fail(req, "400 Bad Request", "box has no load cell");
    }
    boxes_tare(box);
    return reply_ok(req);
}

static esp_err_t h_calibrate(httpd_req_t *req)
{
    int box;
    float g;
    if (!q_int(req, "box", &box) || !boxes_has_cell(box) || !q_float(req, "grams", &g) || g <= 0.0f) {
        return reply_fail(req, "400 Bad Request", "need box with a cell and grams > 0");
    }
    boxes_calibrate(box, g);
    return reply_ok(req);
}

static esp_err_t h_box_clear(httpd_req_t *req)
{
    int box;
    if (!q_int(req, "box", &box) || !boxes_has_cell(box)) {
        return reply_fail(req, "400 Bad Request", "box has no load cell");
    }
    boxes_clear_check(box);
    return reply_ok(req);
}

static esp_err_t h_jog(httpd_req_t *req)
{
    if (!KK_MOTOR_CONNECTED) {
        return reply_fail(req, "409 Conflict", "motor not connected (KK_MOTOR_CONNECTED is 0)");
    }
    float mm;
    int dir = 1;
    if (!q_float(req, "mm", &mm) || mm <= 0.0f) {
        return reply_fail(req, "400 Bad Request", "need mm > 0");
    }
    q_int(req, "dir", &dir);
    if (router_busy()) {
        return reply_fail(req, "409 Conflict", "a route is already running");
    }
    if (!router_jog(mm, dir >= 0 ? 1 : -1)) {
        return reply_fail(req, "409 Conflict", "could not jog");
    }
    return reply_ok(req);
}

static esp_err_t h_arm(httpd_req_t *req)
{
    int box, out = 1;
    if (!q_int(req, "box", &box) || !box_has_arm(box)) {
        return reply_fail(req, "400 Bad Request", "box has no arm");
    }
    q_int(req, "out", &out);
    if (!router_arm(box, out != 0)) {
        return reply_fail(req, "409 Conflict", "a route is already running");
    }
    return reply_ok(req);
}

// Bench test: one servo straight to an angle, with the pulse read back.
static esp_err_t h_servotest(httpd_req_t *req)
{
    int box;
    float deg;
    if (!q_int(req, "box", &box) || !box_has_arm(box) || !q_float(req, "deg", &deg)) {
        return reply_fail(req, "400 Bad Request", "need box (2-4) and deg");
    }
    if (router_busy()) {
        return reply_fail(req, "409 Conflict", "a route is already running");
    }
    int us = 0;
    if (!arm_test_angle(box, deg, &us)) {
        return reply_fail(req, "400 Bad Request", "deg must be 0 to 180");
    }
    int want = (int)(KK_SERVO_MIN_US + (KK_SERVO_MAX_US - KK_SERVO_MIN_US) * (deg / 180.0f));
    char m[96];
    snprintf(m, sizeof m, "{\"ok\":true,\"deg\":%.0f,\"pulse_us\":%d,\"expected_us\":%d}", deg, us, want);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, m);
}

static esp_err_t h_armcfg(httpd_req_t *req)
{
    int box;
    float rest, out;
    if (!q_int(req, "box", &box) || !box_has_arm(box)
        || !q_float(req, "rest", &rest) || !q_float(req, "out", &out)) {
        return reply_fail(req, "400 Bad Request", "need box, rest and out");
    }
    if (router_busy()) {
        return reply_fail(req, "409 Conflict", "a route is already running");
    }
    if (!arm_set_angles(box, rest, out)) {
        return reply_fail(req, "400 Bad Request", "angles must be 0 to 180");
    }
    return reply_ok(req);
}

// Find an arm's 0 point: /api/armzero?box=3&deg=40 moves the servo there,
// adding &save=1 stores it as the arm's 0.
static esp_err_t h_armzero(httpd_req_t *req)
{
    int box, save = 0;
    float deg;
    if (!q_int(req, "box", &box) || !box_has_arm(box) || !q_float(req, "deg", &deg)) {
        return reply_fail(req, "400 Bad Request", "need box (2-4) and deg");
    }
    q_int(req, "save", &save);
    if (router_busy()) {
        return reply_fail(req, "409 Conflict", "a route is already running");
    }
    if (!arm_zero(box, deg, save != 0)) {
        return reply_fail(req, "400 Bad Request", "deg must be 0 to 180");
    }
    return reply_ok(req);
}

static esp_err_t h_motor(httpd_req_t *req)
{
    if (!router_motor_off()) {
        return reply_fail(req, "409 Conflict", "a route is already running");
    }
    return reply_ok(req);
}

static esp_err_t h_log_json(httpd_req_t *req)
{
    int n = 20;
    q_int(req, "n", &n);
    if (n < 1) n = 1;
    if (n > KK_LOG_CAPACITY) n = KK_LOG_CAPACITY;
    int count = runlog_count();

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_send_chunk(req, "[", 1);
    bool first = true;
    for (int i = count - 1; i >= 0 && i >= count - n; i--) {
        run_entry_t e;
        if (!runlog_get(i, &e)) continue;
        int len = snprintf(s_buf, sizeof s_buf,
            "%s{\"id\":%lu,\"t\":%lld,\"start\":%d,\"box\":%d,\"result\":\"%s\",\"ms\":%lu,"
            "\"g0\":%.1f,\"g1\":%.1f,\"mm\":%.0f}",
            first ? "" : ",", (unsigned long)e.id, (long long)e.t_start_ms, e.start, e.box,
            runlog_result_str(e.result), (unsigned long)e.duration_ms, e.g_before, e.g_after, e.mm_moved);
        httpd_resp_send_chunk(req, s_buf, len);
        first = false;
    }
    httpd_resp_send_chunk(req, "]", 1);
    return httpd_resp_send_chunk(req, NULL, 0);
}

static esp_err_t h_log_csv(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/csv");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=\"kinetic_kitchen_runs.csv\"");
    const char *hdr = "run_id,start_uptime_ms,start,box,result,duration_ms,g_before,g_after,belt_mm\r\n";
    httpd_resp_send_chunk(req, hdr, strlen(hdr));
    int count = runlog_count();
    for (int i = 0; i < count; i++) {
        run_entry_t e;
        if (!runlog_get(i, &e)) continue;
        int len = snprintf(s_buf, sizeof s_buf, "%lu,%lld,%d,%d,%s,%lu,%.1f,%.1f,%.0f\r\n",
                           (unsigned long)e.id, (long long)e.t_start_ms, e.start, e.box,
                           runlog_result_str(e.result), (unsigned long)e.duration_ms,
                           e.g_before, e.g_after, e.mm_moved);
        httpd_resp_send_chunk(req, s_buf, len);
    }
    return httpd_resp_send_chunk(req, NULL, 0);
}

static esp_err_t h_log_clear(httpd_req_t *req)
{
    runlog_clear();
    return reply_ok(req);
}

static esp_err_t h_sim(httpd_req_t *req)
{
#if KK_SIM_MODE
    int box;
    float g = 0.0f;
    if (!q_int(req, "box", &box) || !boxes_has_cell(box)) {
        return reply_fail(req, "400 Bad Request", "box has no load cell");
    }
    q_float(req, "grams", &g);
    hx711_sim_set_grams(boxes_cell_index(box), g);
    return reply_ok(req);
#else
    return reply_fail(req, "404 Not Found", "simulation is off");
#endif
}

// ---- server ----
void web_server_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_uri_handlers = 24;
    cfg.stack_size = 8192;
    cfg.lru_purge_enable = true;
    ESP_ERROR_CHECK(httpd_start(&s_server, &cfg));

    const httpd_uri_t uris[] = {
        { .uri = "/",              .method = HTTP_GET,  .handler = h_index },
        { .uri = "/api/status",    .method = HTTP_GET,  .handler = h_status },
        { .uri = "/api/route",     .method = HTTP_POST, .handler = h_route },
        { .uri = "/api/stop",      .method = HTTP_POST, .handler = h_stop },
        { .uri = "/api/tare",      .method = HTTP_POST, .handler = h_tare },
        { .uri = "/api/calibrate", .method = HTTP_POST, .handler = h_calibrate },
        { .uri = "/api/box/clear", .method = HTTP_POST, .handler = h_box_clear },
        { .uri = "/api/jog",       .method = HTTP_POST, .handler = h_jog },
        { .uri = "/api/arm",       .method = HTTP_POST, .handler = h_arm },
        { .uri = "/api/armcfg",    .method = HTTP_POST, .handler = h_armcfg },
        { .uri = "/api/servotest", .method = HTTP_POST, .handler = h_servotest },
        { .uri = "/api/armzero",   .method = HTTP_POST, .handler = h_armzero },
        { .uri = "/api/motor",     .method = HTTP_POST, .handler = h_motor },
        { .uri = "/api/log",       .method = HTTP_GET,  .handler = h_log_json },
        { .uri = "/api/log.csv",   .method = HTTP_GET,  .handler = h_log_csv },
        { .uri = "/api/log/clear", .method = HTTP_POST, .handler = h_log_clear },
        { .uri = "/api/sim",       .method = HTTP_POST, .handler = h_sim },
    };
    for (size_t i = 0; i < sizeof uris / sizeof uris[0]; i++) {
        ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &uris[i]));
    }
    ESP_LOGI(TAG, "dashboard server started");
}
