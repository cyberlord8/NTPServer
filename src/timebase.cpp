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

static uint64_t g_last_pps_edge_us = 0;
static uint32_t g_pps_edges_seen  = 0;

namespace {
constexpr uint32_t USEC_PER_SEC = 1000000u;
constexpr uint64_t NTP_UNIX_EPOCH_DELTA = 2208988800ULL; // 1900->1970
static bool g_last_set_used_pps = false;

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

bool timebase_get_last_set_used_pps()
{
    if (!g_tb.inited || !g_tb.lock) return false;

    const uint32_t save = lock_tb();
    const bool v = g_last_set_used_pps;
    unlock_tb(save);
    return v;
}

void timebase_note_pps_edge_us(uint64_t edge_us)
{
    if (!g_tb.inited || !g_tb.lock) return;

    const uint32_t save = lock_tb();

    // telemetry
    g_last_pps_edge_us = edge_us;
    g_pps_edges_seen++;

    // PPS-tick the timebase once we have a baseline.
    if (g_tb.have_time && g_tb.base_us != 0 && edge_us > g_tb.base_us) {

        const uint64_t dt_us = edge_us - g_tb.base_us;

        // Accept ~1 second step (allows some jitter)
        if (dt_us >= 900000ULL && dt_us <= 1100000ULL) {
            g_tb.base_unix += 1;
            g_tb.base_us    = edge_us;

            g_last_set_used_pps = true;
            g_tb.synced = true; // later you can require GPS LOCKED
        }
    }

    unlock_tb(save);
}

uint64_t timebase_get_last_pps_edge_us()
{
    if (!g_tb.inited || !g_tb.lock) return 0;

    const uint32_t save = lock_tb();
    const uint64_t v = g_last_pps_edge_us;
    unlock_tb(save);
    return v;
}

uint32_t timebase_get_pps_edges_seen()
{
    if (!g_tb.inited || !g_tb.lock) return 0;

    const uint32_t save = lock_tb();
    const uint32_t v = g_pps_edges_seen;
    unlock_tb(save);
    return v;
}

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
    g_last_pps_edge_us = 0;
    g_pps_edges_seen = 0;
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

    bool used_pps = false;
    const uint64_t now_us = time_us_64();
    uint64_t snapped_us = 0;

    // First, check for the most recent PPS edge to snap time, if available
    {
        const uint32_t save = lock_tb();
        const uint64_t last_pps = g_last_pps_edge_us;
        unlock_tb(save);

        if (last_pps != 0 && now_us >= last_pps) {
            const uint64_t age_us = now_us - last_pps;
            if (age_us < 990000ULL) {  // Accept only fresh PPS edges
                snapped_us = last_pps;
                used_pps = true;
            }
        }
    }

    // Fallback: If no fresh PPS, use current time to snap to the second
    if (snapped_us == 0) {
        snapped_us = snap_us_to_second(now_us);
        used_pps = false;
    }

    const uint32_t save = lock_tb();
    const bool have = g_tb.have_time;
    const uint64_t cur_unix = g_tb.base_unix;
    const uint64_t cur_base_us = g_tb.base_us;
    unlock_tb(save);

    // If the time is already synced and within the same second, do not reset the baseline.
    if (have && cur_unix == unix_utc_seconds) {
        if (used_pps) {
            // If PPS is active, avoid resetting baseline time (prevent phase shifts).
            return;
        }
    }

    // Update baseline time with the new second if it's not the same or if PPS stepping is active.
    const uint32_t save2 = lock_tb();
    g_tb.base_unix = unix_utc_seconds;
    g_tb.base_us = snapped_us;
    g_tb.have_time = true;
    g_tb.synced = true; // Later, refine this with additional checks if required.
    g_last_set_used_pps = used_pps; // Track if PPS was used in this update
    unlock_tb(save2);
}

// void timebase_on_gps_utc_unix(uint64_t unix_utc_seconds) {
//     if (!g_tb.inited || !g_tb.lock) return;

//     bool used_pps = false;
//     const uint64_t now_us = time_us_64();

//     // Prefer the most recent PPS edge as the true second boundary when it is fresh.
//     uint64_t snapped_us = 0;
//     {
//         const uint32_t save = lock_tb();
//         const uint64_t last_pps = g_last_pps_edge_us;
//         unlock_tb(save);

//         if (last_pps != 0 && now_us >= last_pps) {
//             const uint64_t age_us = now_us - last_pps;
//             if (age_us < 990000ULL) {
//                 snapped_us = last_pps;
//                 used_pps = true;
//             }
//         }
//     }

//     // Fallback: remove NMEA arrival latency bias using local snap
//     if (snapped_us == 0) {
//         snapped_us = snap_us_to_second(now_us);
//         used_pps = false;
//     }

//     const uint32_t save = lock_tb();
//     const bool have = g_tb.have_time;
//     const uint64_t cur_unix = g_tb.base_unix;
//     const uint64_t cur_base_us = g_tb.base_us;
//     unlock_tb(save);

//     // Prevent resetting the baseline if it's the same UTC second
//     if (have && cur_unix == unix_utc_seconds && used_pps) {
//         // If PPS is used and we're still in the same second, avoid modifying base_us
//         return;  // Same second; don't reset baseline again if PPS is active.
//     }

//     // Now we update the baseline values (only if the second changed or PPS stepping is active)
//     const uint32_t save2 = lock_tb();
//     g_tb.base_unix = unix_utc_seconds;
//     g_tb.base_us   = snapped_us;  // Only update base_us when the second has changed or on PPS update
//     g_tb.have_time = true;
//     g_tb.synced    = true;  // later: require GPS Locked + PPS discipline
//     g_last_set_used_pps = used_pps;   // Track PPS usage
//     unlock_tb(save2);
// }

// void timebase_on_gps_utc_unix(uint64_t unix_utc_seconds) {
//     if (!g_tb.inited || !g_tb.lock) return;

//     const uint64_t now_us = time_us_64();

//     const uint32_t save = lock_tb();

//     const bool have = g_tb.have_time;
//     const bool pps_active = (g_pps_edges_seen > 0) && (g_tb.base_us != 0);

//     if (have && pps_active) {
//         // PPS is already the metronome. Only correct the UTC LABEL if we're clearly off.
//         // Accept within +/- 1 second to avoid fighting edge-alignment / UART timing.
//         const uint64_t cur_unix = g_tb.base_unix;

//         if (cur_unix == unix_utc_seconds ||
//             cur_unix + 1 == unix_utc_seconds ||
//             (cur_unix > 0 && cur_unix - 1 == unix_utc_seconds)) {
//             // label agrees closely enough; do nothing
//             unlock_tb(save);
//             return;
//         }

//         // Large jump: correct the label, but DO NOT move base_us (PPS keeps phase).
//         g_tb.base_unix = unix_utc_seconds;
//         g_tb.have_time = true;
//         g_tb.synced = true;
//         // leave g_last_set_used_pps alone here; PPS tick owns it
//         unlock_tb(save);
//         return;
//     }

//     // Startup / no PPS: establish an initial baseline (phase is best-effort here)
//     uint64_t snapped_us = 0;
//     if (g_last_pps_edge_us != 0 && now_us >= g_last_pps_edge_us) {
//         const uint64_t age_us = now_us - g_last_pps_edge_us;
//         if (age_us < 990000ULL) {
//             snapped_us = g_last_pps_edge_us;
//             g_last_set_used_pps = true;
//         }
//     }
//     if (snapped_us == 0) {
//         snapped_us = now_us - (now_us % USEC_PER_SEC);
//         g_last_set_used_pps = false;
//     }

//     g_tb.base_unix = unix_utc_seconds;
//     g_tb.base_us   = snapped_us;
//     g_tb.have_time = true;
//     g_tb.synced    = true;

//     unlock_tb(save);
// }

// void timebase_on_gps_utc_unix(uint64_t unix_utc_seconds) {
//     if (!g_tb.inited || !g_tb.lock) return;

//     bool used_pps = false;
//     const uint64_t now_us = time_us_64();

//     // Prefer the most recent PPS edge as the true second boundary when it is fresh.
//     uint64_t snapped_us = 0;
//     {
//         const uint32_t save = lock_tb();
//         const uint64_t last_pps = g_last_pps_edge_us;
//         unlock_tb(save);

//         if (last_pps != 0 && now_us >= last_pps) {
//             const uint64_t age_us = now_us - last_pps;
//             if (age_us < 990000ULL) {  // 990ms threshold for PPS freshness
//                 snapped_us = last_pps;
//                 used_pps = true;
//             }
//         }
//     }

//     // Fallback: remove NMEA arrival latency bias using local snap
//     if (snapped_us == 0) {
//         snapped_us = snap_us_to_second(now_us);
//         used_pps = false;
//     }

//     const uint32_t save = lock_tb();
//     const bool have = g_tb.have_time;
//     const uint64_t cur_unix = g_tb.base_unix;
//     unlock_tb(save);

//     // Prevent resetting the baseline if it's the same UTC second
//     // (this avoids re-labeling and unnecessary phase shifts when PPS is in effect)
//     if (have && cur_unix == unix_utc_seconds) {
//         return; // Same second; don't reset baseline again.
//     }

//     // Update the timebase with new values
//     g_tb.base_unix = unix_utc_seconds;
//     g_tb.base_us   = snapped_us;
//     g_tb.have_time = true;
//     g_tb.synced    = true;  // later: require GPS Locked + PPS discipline
//     g_last_set_used_pps = used_pps;  // Track PPS usage only if it updated the timebase

//     unlock_tb(save);
// }

// void timebase_on_gps_utc_unix(uint64_t unix_utc_seconds) {
//     if (!g_tb.inited || !g_tb.lock) return;

//     bool used_pps = false;
//     const uint64_t now_us = time_us_64();

//     // Prefer the most recent PPS edge as the true second boundary when it is fresh.
//     uint64_t snapped_us = 0;
//     {
//         const uint32_t save = lock_tb();
//         const uint64_t last_pps = g_last_pps_edge_us;
//         unlock_tb(save);

//         if (last_pps != 0 && now_us >= last_pps) {
//             const uint64_t age_us = now_us - last_pps;
//             if (age_us < 990000ULL) {
//                 snapped_us = last_pps;
//                 used_pps = true;
//             }
//         }
//     }

//     // Fallback: remove NMEA arrival latency bias using local snap
//     if (snapped_us == 0) {
//         snapped_us = snap_us_to_second(now_us);
//         used_pps = false;
//     }

//     const uint32_t save = lock_tb();
//     const bool have = g_tb.have_time;
//     const uint64_t cur_unix = g_tb.base_unix;
//     unlock_tb(save);

//     // Prevent resetting the baseline if it's the same UTC second
//     if (have && cur_unix == unix_utc_seconds) {
//         return; // Same second; don't reset baseline again.
//     }

//     g_tb.base_unix = unix_utc_seconds;
//     g_tb.base_us   = snapped_us;
//     g_tb.have_time = true;
//     g_tb.synced    = true;  // later: require GPS Locked + PPS discipline
//     g_last_set_used_pps = used_pps;   // Track PPS usage
//     unlock_tb(save);
// }

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

