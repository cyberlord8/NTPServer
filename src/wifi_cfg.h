#pragma once
#include <cstdint>

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

void wifi_cfg_init();

// call this before connect (or right after init)
void wifi_cfg_set_static_ipv4(const WifiStaticIpv4* cfg);

bool wifi_cfg_connect_blocking(const char* ssid, const char* password, uint32_t timeout_ms);
WifiStatus wifi_cfg_get_status();
