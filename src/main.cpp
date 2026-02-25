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
#include "pico/stdlib.h"
#include "pico/time.h"
#include <cstdint>
#include <cstring>
#include <cstdlib>

#include "gps_uart.h"
#include "led.h"
#include "gps_state.h"
#include "ui_console.h"
#include "temp.h"
#include "uptime.h"

#include "wifi_secrets.h"
#include "wifi_cfg.h"
#include "ntp_server.h"
#include "timebase.h"

#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "pps.h"

// Pico W only
#ifdef CYW43_WL_GPIO_LED_PIN
#include "pico/cyw43_arch.h"
#endif

static bool cfg_wifi()
{
#ifdef CYW43_WL_GPIO_LED_PIN
    WifiStatus ws = wifi_cfg_get_status();
    led_set_cyw43_ready(ws.cyw43_ok);
#endif

    wifi_cfg_init();
    // const WifiStaticIpv4 s = wifi_cfg_make_static_ipv4_192_168_0(123,1,200);
    // wifi_cfg_set_static_ipv4(&s);
    // bool ok = wifi_cfg_connect_blocking(WIFI_SSID, WIFI_PASSWORD, 15000);

    WifiStaticIpv4 s{};
    s.ip[0]=W_IPAddress.OCTET_1; s.ip[1]=W_IPAddress.OCTET_2; s.ip[2]=W_IPAddress.OCTET_3; s.ip[3]=W_IPAddress.OCTET_4;
    s.netmask[0]=255; s.netmask[1]=255; s.netmask[2]=255; s.netmask[3]=0;
    s.gateway[0]=192; s.gateway[1]=168; s.gateway[2]=0; s.gateway[3]=1;
    // optional DNS:
    s.dns[0]=192; s.dns[1]=168; s.dns[2]=0; s.dns[3]=200;

    wifi_cfg_set_static_ipv4(&s);

    bool ok = wifi_cfg_connect_blocking(WIFI_SSID, WIFI_PASSWORD, 15000);
    printf("WIFI: %s\r\n", ok ? "CONNECTED" : "FAILED");
    return ok;
}

static void setup_led(repeating_timer_t &timer)
{
    int rc = pico_led_init();
    hard_assert(rc == PICO_OK);
    add_repeating_timer_ms(50, pulse_cb, (void*)&g_state, &timer);
    // add_repeating_timer_ms(50, pulse_cb, nullptr, &timer);
}

static void handle_nmea()
{
    char line[256];
    while (GpsUart::get_line(line, sizeof(line))) {
        // printf(".");
        update_from_nmea(line);
    }
}

static void redraw_dashboard(absolute_time_t &next_ui)
{
    if (absolute_time_diff_us(get_absolute_time(), next_ui) <= 0) {
        dashboard_draw();
        next_ui = make_timeout_time_ms(500);
    }
}

// static constexpr uint PPS_GPIO = 16;

// // Updated in IRQ (keep these small + deterministic)
// static volatile uint32_t g_pps_edges = 0;
// static volatile uint32_t g_pps_last_interval_us = 0;

// static void pps_irq_callback(uint gpio, uint32_t events)
// {
//     (void)events;
//     if (gpio != PPS_GPIO) return;

//     static uint32_t last_edge_us_32 = 0;  // 32-bit is fine for ~71 minutes wrap
//     uint32_t now_us_32 = (uint32_t)time_us_64();

//     // interval between rising edges (wrap-safe for uint32_t)
//     uint32_t dt = now_us_32 - last_edge_us_32;
//     last_edge_us_32 = now_us_32;

//     g_pps_last_interval_us = dt;
//     g_pps_edges++;
// }

// static void pps_test_init()
// {
//     gpio_init(PPS_GPIO);
//     gpio_set_dir(PPS_GPIO, GPIO_IN);

//     // PPS is typically a driven signal; pull-down helps avoid floating if not present.
//     gpio_pull_down(PPS_GPIO);

//     // Rising edge IRQ on GPIO16
//     gpio_set_irq_enabled_with_callback(PPS_GPIO, GPIO_IRQ_EDGE_RISE, true, &pps_irq_callback);

//     printf("PPS TEST: LISTENING ON GPIO%u (RISING EDGE)\r\n", PPS_GPIO);
// }

// static void pps_test_poll_and_print()
// {
//     // Print once per second
//     static absolute_time_t next = nil_time;
//     if (is_nil_time(next)) next = make_timeout_time_ms(1000);

//     if (absolute_time_diff_us(get_absolute_time(), next) <= 0) {
//         uint32_t edges = g_pps_edges;
//         uint32_t dt_us = g_pps_last_interval_us;

//         printf("PPS: EDGES=%lu  LAST_DT=%lu us\r\n",
//                (unsigned long)edges,
//                (unsigned long)dt_us);

//         next = make_timeout_time_ms(1000);
//     }
// }

int main() {
    repeating_timer_t timer;
    absolute_time_t next_ui = make_timeout_time_ms(500);

    stdio_init_all();
    // Give USB CDC time to enumerate
    sleep_ms(1500);

    // OPTIONAL but very useful 
    // to visualize dashboard, from terminal app run: 
    //      picocom /dev/ttyACM0 -b 115200
    // If you're using USB stdio, wait for a terminal connection.
    // Comment this out if you want it to run headless.
#ifdef PICO_STDIO_USB
    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }
#endif
    //optional sleep so you have time to start terminal application to see bootup process
    // sleep_ms(10000);
    printf("\x1b[?25l"); // hide cursor
    printf("PICO NTPServer starting...\r\n");

    temp_init();
    uptime_init();
    timebase_init();

    //NOTE: might need to rethink this once we get USB RNDIS Network device working
    if (cfg_wifi()) {
      ntp_server_init();
    } else
      n_status = false;

    led_bind_state(&g_state);
    setup_led(timer);


    GpsUart::init(9600, /*rx_gpio=*/1, /*tx_gpio=*/0);
    pps_init(16);

    while (true) {
        // printf(".");
        handle_nmea();

        gps_state_service();

        led_service();

        redraw_dashboard(next_ui);

        tight_loop_contents();
    }
}

