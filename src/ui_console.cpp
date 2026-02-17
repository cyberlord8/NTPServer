#include <cstdio>

#include "ui_console.h"
#include "gps_state.h"
#include "ntp_server.h"
#include "temp.h"
#include "uptime.h"

#include "wifi_cfg.h"
#include "lwip/ip4_addr.h"

void dashboard_draw() {

    float raw = read_temp_c();
    float smooth = temp_ema_update_throttled(raw);
    char up[32];
    uptime_format(up, sizeof(up));
    WifiStatus ws = wifi_cfg_get_status();


    // setup colors
    const char* st = state_str(g_state);
    const char* g_col = (g_state==GPSDeviceState::Acquired || g_state==GPSDeviceState::Locked) ? "\x1b[32m" :
                      (g_state==GPSDeviceState::Acquiring || g_state==GPSDeviceState::Booting) ? "\x1b[33m" :
                      "\x1b[31m";

    const char* n_col = (n_status==true) ? "\x1b[32m" : "\x1b[31m";

    // ANSI: clear + home
    printf("\x1b[H\x1b[2J");
    
    // header
    printf("NTPServer (Pico W)  |  GPS/NTP Status\r\n");
    printf("------------------------------------------------------------\r\n");

    //stats
    printf("Device State : %s%s\x1b[0m\r\n", g_col, st);

    printf("RMC Valid    : %s\r\n", gps.rmc_valid ? "YES" : "NO");
    printf("GGA Fix      : %s\r\n", gps.gga_fix ? "YES" : "NO");
    printf("Satellites   : %d\r\n", gps.sats);
    printf("HDOP         : %.1f\r\n", gps.hdop);
    printf("UTC (ZDA)    : %s\r\n", gps.last_zda[0] ? gps.last_zda : "(waiting)");
    printf("UTC (RMC)    : %s\r\n", gps.last_rmc_time[0] ? gps.last_rmc_time : "(waiting)");
    
    printf("\r\n");
    printf("CPU Temp     : %.2f C\n", smooth);
    printf("UPTIME       : %s\n", up);
    printf("\r\n");

    //Network Stats
    printf("WIFI LINK: %s%s\x1b[0m", n_col, ws.link_up ? "UP" : "DOWN");

    if (ws.has_ip) {
        ip4_addr_t ip{ ws.ip_addr_be };
        printf(" - IP: %s\r\n", ip4addr_ntoa(&ip));
    } else {
        printf(" - IP: (none)\r\n");
    }    
    if(n_status)
        printf("          NTP Port: %d\r\n", 123);
    printf("\r\n");

    printf("Notes:\r\n");
    printf(" - LOCKED will be PPS disciplined once PPS is wired.\r\n");

    fflush(stdout);
}