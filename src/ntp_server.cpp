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
#include "ntp_server.h"

#include <cstring>

#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "lwip/ip_addr.h"

#include "timebase.h"

static constexpr uint16_t NTP_PORT = 123;
static constexpr int8_t   NTP_PRECISION = -20;        // ~1 us-ish (placeholder)
static constexpr uint32_t NTP_REFID_GPS = 0x47505300;  // "GPS\0"

static udp_pcb* g_pcb = nullptr;
bool n_status = false;

#pragma pack(push, 1)
struct NtpPacket {
    uint8_t  li_vn_mode;     // LI (2) | VN (3) | Mode (3)
    uint8_t  stratum;
    uint8_t  poll;
    int8_t   precision;
    uint32_t root_delay;
    uint32_t root_dispersion;
    uint32_t ref_id;

    uint32_t ref_ts_s;
    uint32_t ref_ts_f;

    uint32_t orig_ts_s;
    uint32_t orig_ts_f;

    uint32_t recv_ts_s;
    uint32_t recv_ts_f;

    uint32_t tx_ts_s;
    uint32_t tx_ts_f;
};
#pragma pack(pop)

static inline uint8_t ntp_extract_vn(uint8_t li_vn_mode) {
    return (li_vn_mode >> 3) & 0x07;
}

static inline uint8_t ntp_make_li_vn_mode(uint8_t li, uint8_t vn, uint8_t mode) {
    return static_cast<uint8_t>(((li & 0x03u) << 6) | ((vn & 0x07u) << 3) | (mode & 0x07u));
}

static inline uint8_t ntp_normalize_vn(uint8_t vn) {
    // Accept v3/v4, clamp everything else to v4
    if (vn < 3 || vn > 4) return 4;
    return vn;
}

static inline uint32_t hton32(uint32_t x) { return lwip_htonl(x); }

static bool ntp_get_time(uint32_t* s, uint32_t* f) {
    // timebase_now_ntp returns seconds+fraction in host order
    return timebase_now_ntp(s, f);
}

static void ntp_fill_response(NtpPacket* rsp,
                              const NtpPacket* req,
                              uint32_t t2s, uint32_t t2f,
                              uint32_t t3s, uint32_t t3f) {
    const uint8_t vn = ntp_normalize_vn(ntp_extract_vn(req->li_vn_mode));

    const bool have_time = timebase_have_time();
    const bool synced    = timebase_is_synced();

    // LI: 0 = no warning, 3 = alarm/unsynchronized
    const uint8_t li = synced ? 0u : 3u;

    rsp->li_vn_mode = ntp_make_li_vn_mode(li, vn, /*mode=*/4u); // server mode
    rsp->stratum    = have_time ? (synced ? 1 : 2) : 16;
    rsp->poll       = req->poll;
    rsp->precision  = NTP_PRECISION;

    rsp->root_delay      = hton32(0);
    rsp->root_dispersion = hton32(0);
    rsp->ref_id          = hton32(NTP_REFID_GPS);

    // Originate timestamp: echo client's transmit timestamp verbatim (already network order)
    rsp->orig_ts_s = req->tx_ts_s;
    rsp->orig_ts_f = req->tx_ts_f;

    // Reference/Receive/Transmit timestamps: host -> network
    rsp->ref_ts_s  = hton32(t2s);
    rsp->ref_ts_f  = hton32(t2f);

    rsp->recv_ts_s = hton32(t2s);
    rsp->recv_ts_f = hton32(t2f);

    rsp->tx_ts_s   = hton32(t3s);
    rsp->tx_ts_f   = hton32(t3f);
}

static void on_ntp_rx(void*,
                      udp_pcb* pcb,
                      pbuf* p,
                      const ip_addr_t* addr,
                      u16_t port) {
    if (!p) return;

    if (p->tot_len < sizeof(NtpPacket)) {
        pbuf_free(p);
        return;
    }

    NtpPacket req{};
    const u16_t copied = pbuf_copy_partial(p, &req, sizeof(req), 0);
    pbuf_free(p);

    if (copied != sizeof(req)) return;

    // Only respond to client mode (3)
    const uint8_t mode = req.li_vn_mode & 0x07u;
    if (mode != 3u) return;

    uint32_t t2s = 0, t2f = 0;
    if (!ntp_get_time(&t2s, &t2f)) return;

    uint32_t t3s = 0, t3f = 0;
    if (!ntp_get_time(&t3s, &t3f)) return;

    NtpPacket rsp{};
    ntp_fill_response(&rsp, &req, t2s, t2f, t3s, t3f);

    pbuf* out = pbuf_alloc(PBUF_TRANSPORT, sizeof(rsp), PBUF_RAM);
    if (!out) return;

    std::memcpy(out->payload, &rsp, sizeof(rsp));
    (void)udp_sendto(pcb, out, addr, port);
    pbuf_free(out);
}

void ntp_server_init() {
    if (g_pcb) {
        // Already initialized; keep status as-is.
        return;
    }

    g_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
    if (!g_pcb) {
        n_status = false;
        return;
    }

    if (udp_bind(g_pcb, IP_ANY_TYPE, NTP_PORT) != ERR_OK) {
        udp_remove(g_pcb);
        g_pcb = nullptr;
        n_status = false;
        return;
    }

    udp_recv(g_pcb, on_ntp_rx, nullptr);
    n_status = true;
}

// void ntp_server_deinit() {
//     if (!g_pcb) {
//         n_status = false;
//         return;
//     }

//     // Unregister callback then remove PCB
//     udp_recv(g_pcb, nullptr, nullptr);
//     udp_remove(g_pcb);
//     g_pcb = nullptr;
//     n_status = false;
// }

bool ntp_server_is_running() {
    return (g_pcb != nullptr) && n_status;
}

