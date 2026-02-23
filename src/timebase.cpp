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
#include "timebase.h"

#include "pico/time.h"
#include "hardware/sync.h"

namespace {

constexpr uint32_t USEC_PER_SEC = 1000000u;
constexpr uint64_t NTP_UNIX_EPOCH_DELTA = 2208988800ULL; // 1900->1970

struct TimebaseState {
    // Baseline: UTC unix seconds at baseline, and "snapped" local microsecond counter
    uint64_t base_unix = 0;
    uint64_t base_us   = 0;

    bool have_time = false;

    // For now, treat "synced" as "have_time" (tighten later when PPS is used)
    bool synced = false;

    // crude but effective protection
    spin_lock_t* lock = nullptr;
    uint32_t lock_num = 0;
    bool inited = false;
};

TimebaseState g_tb;

inline uint32_t lock_tb() {
    // Assumes timebase_init() was called once at boot.
    return spin_lock_blocking(g_tb.lock);
}

inline void unlock_tb(uint32_t save) {
    spin_unlock(g_tb.lock, save);
}

inline uint64_t snap_us_to_second(uint64_t t_us) {
    return t_us - (t_us % USEC_PER_SEC);
}

inline uint32_t usec_to_ntp_frac(uint32_t usec) {
    // fraction = usec * 2^32 / 1e6, with 64-bit intermediate
    return static_cast<uint32_t>((static_cast<uint64_t>(usec) << 32) / USEC_PER_SEC);
}

} // namespace

void timebase_init(void) {
    if (g_tb.inited) return;

    g_tb.lock_num = spin_lock_claim_unused(true);
    g_tb.lock = spin_lock_init(g_tb.lock_num);

    const uint32_t save = lock_tb();
    g_tb.base_unix = 0;
    g_tb.base_us   = 0;
    g_tb.have_time = false;
    g_tb.synced    = false;
    unlock_tb(save);

    g_tb.inited = true;
}

void timebase_clear(void) {
    if (!g_tb.inited || !g_tb.lock) return;

    const uint32_t save = lock_tb();
    g_tb.have_time = false;
    g_tb.synced    = false;
    g_tb.base_unix = 0;
    g_tb.base_us   = 0;
    unlock_tb(save);
}

bool timebase_have_time(void) {
    if (!g_tb.inited || !g_tb.lock) return false;

    const uint32_t save = lock_tb();
    const bool v = g_tb.have_time;
    unlock_tb(save);
    return v;
}

bool timebase_is_synced(void) {
    if (!g_tb.inited || !g_tb.lock) return false;

    const uint32_t save = lock_tb();
    const bool v = g_tb.synced;
    unlock_tb(save);
    return v;
}

void timebase_on_gps_utc_unix(uint64_t unix_utc_seconds) {
    if (!g_tb.inited || !g_tb.lock) return;

    // Local monotonic time in us since boot
    const uint64_t now_us = time_us_64();

    // SNAP to the *start* of the current second to remove NMEA arrival latency bias
    const uint64_t snapped_us = snap_us_to_second(now_us);

    const uint32_t save = lock_tb();
    g_tb.base_unix = unix_utc_seconds;
    g_tb.base_us   = snapped_us;
    g_tb.have_time = true;
    g_tb.synced    = true;   // later: require GPS Locked + PPS discipline
    unlock_tb(save);
}

bool timebase_now_unix(uint64_t* unix_seconds, uint32_t* usec) {
    if (!unix_seconds || !usec) return false;
    if (!g_tb.inited || !g_tb.lock) return false;

    uint64_t base_unix = 0;
    uint64_t base_us   = 0;
    bool have = false;

    const uint32_t save = lock_tb();
    base_unix = g_tb.base_unix;
    base_us   = g_tb.base_us;
    have      = g_tb.have_time;
    unlock_tb(save);

    if (!have) return false;

    const uint64_t now_us = time_us_64();
    const uint64_t delta_us = (now_us >= base_us) ? (now_us - base_us) : 0;

    *unix_seconds = base_unix + (delta_us / USEC_PER_SEC);
    *usec = static_cast<uint32_t>(delta_us % USEC_PER_SEC);
    return true;
}

bool timebase_now_ntp(uint32_t* ntp_seconds, uint32_t* ntp_fraction) {
    if (!ntp_seconds || !ntp_fraction) return false;

    uint64_t unix_s = 0;
    uint32_t usec = 0;
    if (!timebase_now_unix(&unix_s, &usec)) return false;

    const uint64_t ntp_s_64 = unix_s + NTP_UNIX_EPOCH_DELTA;
    *ntp_seconds  = static_cast<uint32_t>(ntp_s_64);
    *ntp_fraction = usec_to_ntp_frac(usec);
    return true;
}

