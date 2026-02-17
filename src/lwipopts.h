#pragma once

// Minimal lwIP options for Pico W + pico_cyw43_arch_lwip_threadsafe_background
// Focus: DHCP + DNS + UDP server (NTP), no netconn/socket API.

#ifndef LWIPOPTS_H
#define LWIPOPTS_H

// --- Core ---
#define NO_SYS                         1
#define SYS_LIGHTWEIGHT_PROT           1

// --- Memory / buffers (safe starter values) ---
#define MEM_ALIGNMENT                  4
#define MEM_SIZE                       (16 * 1024)

#define MEMP_NUM_PBUF                  32
#define MEMP_NUM_UDP_PCB               8
#define MEMP_NUM_TCP_PCB               4
#define MEMP_NUM_TCP_PCB_LISTEN        2
#define MEMP_NUM_TCP_SEG               16
#define MEMP_NUM_SYS_TIMEOUT           8

#define PBUF_POOL_SIZE                 32
#define PBUF_POOL_BUFSIZE              1536   // enough for full Ethernet frame

// --- Networking features ---
#define LWIP_ARP                       1
#define LWIP_ETHERNET                  1
#define LWIP_ICMP                      1

#define LWIP_IPV4                      1
#define LWIP_IPV6                      0

#define LWIP_UDP                       1
#define LWIP_TCP                       1
#define LWIP_RAW                       0

// --- DHCP/DNS (STA mode) ---
#define LWIP_DHCP                      1
#define LWIP_DNS                       1
#define DNS_TABLE_SIZE                 4
#define DNS_MAX_NAME_LENGTH            256

// --- Checksums ---
#define LWIP_CHECKSUM_CTRL_PER_NETIF   1

// --- Netconn/socket API (not using it) ---
#define LWIP_NETCONN                   0
#define LWIP_SOCKET                    0

// --- Debug (off by default) ---
#define LWIP_DEBUG                     0

#endif // LWIPOPTS_H
