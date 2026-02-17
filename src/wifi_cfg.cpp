#include "wifi_cfg.h"

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/inet.h"   // lwip_htonl / lwip_ntohl
#include "lwip/dhcp.h"   // dhcp_stop
#include "lwip/dns.h"    // dns_setserver


static bool g_use_static = false;
static WifiStaticIpv4 g_static{};

static WifiStatus g_status{};

static uint32_t ip4_to_be(const uint8_t a[4]) {
    // lwIP stores ip4_addr_t.addr in network byte order
    return lwip_htonl(((uint32_t)a[0] << 24) | ((uint32_t)a[1] << 16) | ((uint32_t)a[2] << 8) | (uint32_t)a[3]);
}

static void refresh_ip_locked() {
    // Must be called inside cyw43_arch_lwip_begin()/end()
    struct netif* nif = netif_default;
    if (!nif) {
        g_status.has_ip = false;
        g_status.ip_addr_be = 0;
        return;
    }

    // "Up" doesn't guarantee DHCP lease yet, so check IP != 0.0.0.0
    const ip4_addr_t* ip = netif_ip4_addr(nif);
    if (ip && !ip4_addr_isany_val(*ip)) {
        g_status.has_ip = true;
        g_status.ip_addr_be = ip->addr; // lwIP stores in network byte order
    } else {
        g_status.has_ip = false;
        g_status.ip_addr_be = 0;
    }
}

void wifi_cfg_set_static_ipv4(const WifiStaticIpv4* cfg) {
    if (cfg) {
        g_static = *cfg;
        g_use_static = true;
    } else {
        g_use_static = false;
    }
}

void wifi_cfg_init() {
    g_status = {};

    int rc = cyw43_arch_init();
    g_status.cyw43_ok = (rc == 0);
    if (!g_status.cyw43_ok) {
        return;
    }

    cyw43_arch_enable_sta_mode();
    g_status.sta_enabled = true;
}

bool wifi_cfg_connect_blocking(const char* ssid, const char* password, uint32_t timeout_ms) {
        // Connect (this call blocks up to timeout_ms internally)
    int rc = cyw43_arch_wifi_connect_timeout_ms(
        ssid,
        password,
        password && password[0] ? CYW43_AUTH_WPA2_AES_PSK : CYW43_AUTH_OPEN,
        timeout_ms
    );
    
    g_status.link_up = (rc == 0);
    if (!g_status.link_up) {
        g_status.has_ip = false;
        g_status.ip_addr_be = 0;
        return false;
    }

    if (g_use_static) {
        cyw43_arch_lwip_begin();
        struct netif* nif = netif_default;
        if (nif) {
            // Stop DHCP client if it was started
            dhcp_stop(nif);

            ip4_addr_t ip, nm, gw;
            ip.addr = ip4_to_be(g_static.ip);
            nm.addr = ip4_to_be(g_static.netmask);
            gw.addr = ip4_to_be(g_static.gateway);

            netif_set_addr(nif, &ip, &nm, &gw);

            // Optional DNS
            if (g_static.dns[0] || g_static.dns[1] || g_static.dns[2] || g_static.dns[3]) {
                ip4_addr_t dns;
                dns.addr = ip4_to_be(g_static.dns);
                dns_setserver(0, &dns);
            }

            // Cache status immediately
            g_status.has_ip = true;
            g_status.ip_addr_be = ip.addr;
        }
        cyw43_arch_lwip_end();
        return true;
    }

    // else: DHCP path (existing)
    if (!g_status.cyw43_ok || !g_status.sta_enabled) {
        return false;
    }
    if (!ssid || !ssid[0]) {
        return false;
    }

    g_status.link_up = (rc == 0);
    if (!g_status.link_up) {
        g_status.has_ip = false;
        g_status.ip_addr_be = 0;
        return false;
    }

    // Wait for DHCP lease / IP (up to timeout_ms total)
    absolute_time_t deadline = make_timeout_time_ms(timeout_ms);

    while (!time_reached(deadline)) {
        cyw43_arch_lwip_begin();
        refresh_ip_locked();
        cyw43_arch_lwip_end();

        if (g_status.has_ip) return true;
        sleep_ms(100);
    }

    // Connected to AP, but no DHCP lease yet
    return false;
}

WifiStatus wifi_cfg_get_status() {
    WifiStatus out = g_status;

    // Try to refresh IP if CYW43 is up (non-blocking)
    if (out.cyw43_ok) {
        cyw43_arch_lwip_begin();
        refresh_ip_locked();
        out = g_status;
        cyw43_arch_lwip_end();
    }

    return out;
}
