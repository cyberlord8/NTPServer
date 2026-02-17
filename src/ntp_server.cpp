#include "ntp_server.h"

#include <cstring>
#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "lwip/ip_addr.h"
#include "timebase.h"

static udp_pcb* g_pcb = nullptr;
bool n_status = false;

#pragma pack(push, 1)
struct NtpPacket {
    uint8_t  li_vn_mode;
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

static uint32_t hton32(uint32_t x) {
    return lwip_htonl(x);
}
static uint32_t ntoh32(uint32_t x) {
    return lwip_ntohl(x);
}

static void on_ntp_rx(void*,
                      udp_pcb* pcb,
                      pbuf* p,
                      const ip_addr_t* addr,
                      u16_t port) {
    if (!p || p->tot_len < sizeof(NtpPacket)) {
        if (p) pbuf_free(p);
        return;
    }

    NtpPacket req{};
    const u16_t copied = pbuf_copy_partial(p, &req, sizeof(req), 0);
    pbuf_free(p);
    if (copied != sizeof(req)) return;

    const uint8_t mode = req.li_vn_mode & 0x07;
    if (mode != 3) return;  // only respond to client mode


    NtpPacket rsp{};
    uint8_t vn = (req.li_vn_mode >> 3) & 0x07;
    if (vn < 3 || vn > 4) vn = 4;
    rsp.li_vn_mode = (0u << 6) | (vn << 3) | 4u;



    uint32_t t2s=0, t2f=0;
    if (!timebase_now_ntp(&t2s, &t2f)) return;

    const bool have_time = timebase_have_time();
    const bool synced    = timebase_is_synced();
    rsp.stratum = have_time ? (synced ? 1 : 2) : 16;

    const uint8_t li = synced ? 0u : 3u;  // 3 = alarm/unsynchronized
    rsp.li_vn_mode = (li << 6) | (vn << 3) | 4u;

    rsp.poll = req.poll;
    rsp.precision = -20;
    rsp.root_delay = hton32(0);
    rsp.root_dispersion = hton32(0);
    rsp.ref_id = hton32(0x47505300UL);

    rsp.orig_ts_s = req.tx_ts_s;
    rsp.orig_ts_f = req.tx_ts_f;

    rsp.recv_ts_s = hton32(t2s);
    rsp.recv_ts_f = hton32(t2f);

    rsp.ref_ts_s = hton32(t2s);
    rsp.ref_ts_f = hton32(t2f);

    uint32_t t3s=0, t3f=0;
    if (!timebase_now_ntp(&t3s, &t3f)) return;
    rsp.tx_ts_s = hton32(t3s);
    rsp.tx_ts_f = hton32(t3f);

    pbuf* out = pbuf_alloc(PBUF_TRANSPORT, sizeof(rsp), PBUF_RAM);
    if (!out) return;
    std::memcpy(out->payload, &rsp, sizeof(rsp));
    err_t err = udp_sendto(pcb, out, addr, port);
    (void)err;

    // udp_sendto(pcb, out, addr, port);
    pbuf_free(out);
}

void ntp_server_init() {
    g_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
    if (!g_pcb) return;

    // NTP port 123
    if (udp_bind(g_pcb, IP_ANY_TYPE, 123) != ERR_OK) {
        udp_remove(g_pcb);
        g_pcb = nullptr;
        n_status = false;
        return;
    }
    
    n_status = true;
    udp_recv(g_pcb, on_ntp_rx, nullptr);
}
