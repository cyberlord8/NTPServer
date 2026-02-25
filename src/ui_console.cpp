/*
 * Pico NTP Server (RP2040 / Pico SDK)
 * Copyright (c) 2026 <Timothy J Millea>.
 *
 * Liability / Warranty Disclaimer:
 * THIS SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include <cstdio>
#include <cstdint>

#include "ui_console.h"
#include "gps_state.h"
#include "ntp_server.h"
#include "temp.h"
#include "uptime.h"

#include "wifi_cfg.h"
#include "lwip/ip4_addr.h"

#include "hardware/timer.h"
#include "pps.h"
#include "timebase.h"

namespace {

// ANSI
static constexpr const char* ANSI_CLR  = "\x1b[0m";
static constexpr const char* ANSI_RED  = "\x1b[31m";
static constexpr const char* ANSI_GRN  = "\x1b[32m";
static constexpr const char* ANSI_YEL  = "\x1b[33m";
static constexpr const char* ANSI_CYN  = "\x1b[36m";  // accent for address/port
static constexpr const char* ANSI_HOME = "\x1b[H";
static constexpr const char* ANSI_CLEAR= "\x1b[2J";
static constexpr const char* ANSI_HIDE_CURSOR = "\x1b[?25l";

static bool g_once = false;

static void draw_pps_block()
{
    const uint32_t edges = pps_get_edges();
    const uint32_t dt_us = pps_get_last_interval_us();
    const uint64_t last_edge_us = pps_get_last_edge_us();
    const uint64_t now_us = time_us_64();

    std::printf("\r\n");
    std::printf("PPS (GPIO16) : %s\r\n", edges ? "DETECTED" : "NO EDGES");
    // std::printf("PPS Edges    : %lu\r\n", (unsigned long)edges);

    if (dt_us > 0) {
        const uint32_t ms = (dt_us + 500) / 1000; // rounded
        std::printf("PPS Interval : %lu ms\r\n", (unsigned long)ms);
    } else {
        std::printf("PPS Interval : (waiting)\r\n");
    }

    // NEW: freshness / age
    if (edges == 0 || last_edge_us == 0) {
        std::printf("PPS Age      : (none)\r\n");
    } else {
        const uint64_t age_us = now_us - last_edge_us;
        const uint32_t age_ms = (uint32_t)((age_us + 500) / 1000); // rounded
        const uint32_t age_s  = age_ms / 1000;
        const uint32_t rem_ms = age_ms % 1000;

        // e.g. "0.214 s" without floats
        std::printf("PPS Age      : %lu.%03lu s\r\n",
                    (unsigned long)age_s,
                    (unsigned long)rem_ms);
        std::printf("PPS AGE      : %s\r\n",
            (timebase_get_last_pps_edge_us() == 0) ? "N/A" : "OK");
        std::printf("PPS EDGE CNT : %lu EDGES\r\n", (unsigned long)timebase_get_pps_edges_seen());
        const uint64_t tb_edge_us = timebase_get_last_pps_edge_us();
        if (tb_edge_us == 0) {
            std::printf("TB PPS AGE   : (none)\r\n");
        } else {
            const uint64_t tb_age_us = now_us - tb_edge_us;
            const uint32_t tb_age_ms = (uint32_t)((tb_age_us + 500) / 1000);
            std::printf("TB PPS AGE   : %lu ms\r\n", (unsigned long)tb_age_ms);
            std::printf("TB BASE SRC  : %s\r\n", timebase_get_last_set_used_pps() ? "PPS" : "SNAP");
        }
    }
}

static inline const char* gps_state_color(GPSDeviceState s)
{
    switch (s) {
        case GPSDeviceState::Acquired:
        case GPSDeviceState::Locked:    return ANSI_GRN;
        case GPSDeviceState::Acquiring:
        case GPSDeviceState::Booting:   return ANSI_YEL;
        case GPSDeviceState::Error:     return ANSI_RED;
    }
    return ANSI_RED;
}

static inline const char* bool_color(bool ok) { return ok ? ANSI_GRN : ANSI_RED; }
static inline const char* yesno(bool v) { return v ? "YES" : "NO"; }

static inline int32_t to_fixed(float v, int32_t scale)
{
    // fixed-point rounding without float printf
    // e.g. scale=100 -> centi-units
    const float scaled = v * (float)scale;
    return (scaled >= 0.0f) ? (int32_t)(scaled + 0.5f) : (int32_t)(scaled - 0.5f);
}

static void print_fixed_2(const char* label, int32_t centi, const char* unit)
{
    // centi -> X.YY
    const int32_t whole = centi / 100;
    int32_t frac = centi % 100;
    if (frac < 0) frac = -frac;
    std::printf("%-12s : %ld.%02ld %s\r\n", label, (long)whole, (long)frac, unit);
}

static void print_fixed_1(const char* label, int32_t deci, const char* unit)
{
    // deci -> X.Y
    const int32_t whole = deci / 10;
    int32_t frac = deci % 10;
    if (frac < 0) frac = -frac;
    std::printf("%-12s : %ld.%01ld %s\r\n", label, (long)whole, (long)frac, unit);
}

static void draw_header()
{
    std::printf("NTPServer (Pico W)  |  GPS/NTP Status\r\n");
    std::printf("------------------------------------------------------------\r\n");
}

static void draw_gps_block()
{
    const char* st = state_str(g_state);
    const char* col = gps_state_color(g_state);

    std::printf("GPS State    : %s%s%s\r\n", col, st, ANSI_CLR);
    std::printf("RMC Valid    : %s\r\n", yesno(gps.rmc_valid));
    std::printf("GGA Fix      : %s\r\n", yesno(gps.gga_fix));
    std::printf("Satellites   : %d\r\n", gps.sats);

    if (gps.hdop >= 0.0f) {
        const int32_t hdop_deci = to_fixed(gps.hdop, 10);
        print_fixed_1("HDOP", hdop_deci, "");
    } else {
        std::printf("%-12s: (waiting)\r\n", "HDOP");
    }

    std::printf("UTC (ZDA)    : %s\r\n", gps.last_zda[0] ? gps.last_zda : "(waiting)");
    std::printf("UTC (RMC)    : %s\r\n", gps.last_rmc_time[0] ? gps.last_rmc_time : "(waiting)");
}

static void draw_sys_block()
{
    const float raw = read_temp_c();
    const float smooth = temp_ema_update_throttled(raw);

    char up[32]{};
    uptime_format(up, sizeof(up));

    std::printf("\r\n");

    const int32_t temp_c_centi = to_fixed(smooth, 100);
    print_fixed_2("CPU Temp", temp_c_centi, "C");

    std::printf("%-12s : %s\r\n", "UPTIME", up);
}

static void draw_net_block()
{
    const WifiStatus ws = wifi_cfg_get_status();

    std::printf("\r\n");

    const char* wifi_col = bool_color(ws.link_up);
    std::printf("WIFI LINK    : %s%s%s", wifi_col, ws.link_up ? "UP" : "DOWN", ANSI_CLR);

    if (ws.has_ip) {
        ip4_addr_t ip{};
        ip.addr = ws.ip_addr_be;

        const char* ip_str = ip4addr_ntoa(&ip);
        std::printf(" - IP: %s%s%s\r\n", ANSI_CYN, ip_str, ANSI_CLR);
    } else {
        std::printf(" - IP: (none)\r\n");
    }

    const char* ntp_col = bool_color(n_status);
    std::printf("NTP SERVER   : %s%s%s\r\n", ntp_col, n_status ? "UP" : "DOWN", ANSI_CLR);

    if (n_status) {
        std::printf("NTP Port     : %s%d%s\r\n", ANSI_CYN, 123, ANSI_CLR);
    }
}

static void draw_notes()
{
    std::printf("\r\n");
    std::printf("Notes:\r\n");
    std::printf(" - Pico XOSC is not temperature-controlled.\r\n");
    std::printf(" - Time will drift slightly as temperature changes.\r\n");
    std::printf(" - For best results, keep the Pico in an enclosure and out of drafty areas.\r\n");
}

} // namespace

void dashboard_draw()
{
    if (!g_once) {
        std::printf("%s", ANSI_HIDE_CURSOR);
        g_once = true;
    }

    // clear + home
    std::printf("%s%s", ANSI_HOME, ANSI_CLEAR);

    draw_header();
    draw_gps_block();
    draw_pps_block();
    draw_sys_block();
    draw_net_block();
    draw_notes();

    std::fflush(stdout);
}
