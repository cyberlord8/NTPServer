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
#pragma once
#include <cstdint>

struct WiFiIPAddress{
uint8_t OCTET_1 = 192;
uint8_t OCTET_2 = 168;
uint8_t OCTET_3 = 0;
uint8_t OCTET_4 = 123;
};

struct WifiStatus {
    bool cyw43_ok;
    bool sta_enabled;
    bool link_up;
    bool has_ip;
    uint32_t ip_addr_be;   // IPv4 in network byte order (lwIP)
};

// static IPv4 config (values in host dotted-quad form, e.g. 192.168.0.123)
struct WifiStaticIpv4 {
    uint8_t ip[4];
    uint8_t netmask[4];
    uint8_t gateway[4];
    uint8_t dns[4];    // optional; 0.0.0.0 means "leave as-is"
};

extern WiFiIPAddress W_IPAddress;
void wifi_cfg_init();

// call this before connect (or right after init)
void wifi_cfg_set_static_ipv4(const WifiStaticIpv4* cfg);

bool wifi_cfg_connect_blocking(const char* ssid, const char* password, uint32_t timeout_ms);
WifiStatus wifi_cfg_get_status();