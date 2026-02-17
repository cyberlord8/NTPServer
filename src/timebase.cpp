#include "timebase.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/sync.h"

static constexpr uint32_t USEC_PER_SEC = 1000000u;
static constexpr uint64_t NTP_UNIX_EPOCH_DELTA = 2208988800ULL; // 1900->1970

// Baseline: unix seconds at baseline, and "snapped" local microsecond counter
static uint64_t g_base_unix = 0;
static uint64_t g_base_us   = 0;

static bool g_have_time = false;

// For now, treat "synced" as "have_time" (you can tighten later when PPS is used)
static bool g_synced = false;

// crude but effective protection
static spin_lock_t* g_lock = nullptr;
static uint32_t g_lock_num = 0;

void timebase_init(void) {
    g_lock_num = spin_lock_claim_unused(true);
    g_lock = spin_lock_init(g_lock_num);

    uint32_t save = spin_lock_blocking(g_lock);
    g_base_unix = 0;
    g_base_us   = 0;
    g_have_time = false;
    g_synced    = false;
    spin_unlock(g_lock, save);
}

void timebase_clear(void) {
    uint32_t save = spin_lock_blocking(g_lock);
    g_have_time = false;
    g_synced = false;
    g_base_unix = 0;
    g_base_us = 0;
    spin_unlock(g_lock, save);
}

bool timebase_have_time(void) {
    uint32_t save = spin_lock_blocking(g_lock);
    bool v = g_have_time;
    spin_unlock(g_lock, save);
    return v;
}

bool timebase_is_synced(void) {
    uint32_t save = spin_lock_blocking(g_lock);
    bool v = g_synced;
    spin_unlock(g_lock, save);
    return v;
}

void timebase_on_gps_utc_unix(uint64_t unix_utc_seconds) {
    // Local monotonic time in us since boot
    uint64_t now_us = time_us_64();

    // SNAP to the *start* of the current second to remove NMEA arrival latency bias
    uint64_t snapped_us = now_us - (now_us % USEC_PER_SEC);

    uint32_t save = spin_lock_blocking(g_lock);
    g_base_unix = unix_utc_seconds;
    g_base_us   = snapped_us;
    g_have_time = true;
    g_synced    = true;   // later: require GPS Locked + PPS discipline
    spin_unlock(g_lock, save);
}

static inline uint32_t usec_to_ntp_frac(uint32_t usec) {
    // fraction = usec * 2^32 / 1e6
    // use 64-bit intermediate
    return (uint32_t)(((uint64_t)usec << 32) / USEC_PER_SEC);
}

bool timebase_now_unix(uint64_t* unix_seconds, uint32_t* usec) {
    if (!unix_seconds || !usec) return false;

    uint64_t base_unix, base_us;
    bool have;

    uint32_t save = spin_lock_blocking(g_lock);
    base_unix = g_base_unix;
    base_us   = g_base_us;
    have      = g_have_time;
    spin_unlock(g_lock, save);

    if (!have) return false;

    uint64_t now_us = time_us_64();
    uint64_t delta_us = (now_us >= base_us) ? (now_us - base_us) : 0;

    *unix_seconds = base_unix + (delta_us / USEC_PER_SEC);
    *usec = (uint32_t)(delta_us % USEC_PER_SEC);
    return true;
}

bool timebase_now_ntp(uint32_t* ntp_seconds, uint32_t* ntp_fraction) {
    if (!ntp_seconds || !ntp_fraction) return false;

    uint64_t unix_s;
    uint32_t usec;
    if (!timebase_now_unix(&unix_s, &usec)) return false;

    uint64_t ntp_s_64 = unix_s + NTP_UNIX_EPOCH_DELTA;
    *ntp_seconds  = (uint32_t)ntp_s_64;
    *ntp_fraction = usec_to_ntp_frac(usec);
    return true;
}










// #include "timebase.h"
// #include "pico/time.h"

// static bool     g_synced = false;
// static uint64_t g_base_unix_s = 0;
// static uint64_t g_base_us = 0;

// // Unix(1970) -> NTP(1900) offset in seconds
// static constexpr uint64_t UNIX_TO_NTP = 2208988800ULL;

// void timebase_init() {
//     g_synced = false;
//     g_base_unix_s = 0;
//     g_base_us = 0;
// }

// void timebase_on_gps_utc_unix(uint64_t unix_seconds) {
//     g_base_unix_s = unix_seconds;
//     g_base_us = time_us_64();
//     g_synced = true;
// }

// bool timebase_is_synced() {
//     return g_synced;
// }

// bool timebase_now_ntp(uint32_t* ntp_seconds, uint32_t* ntp_fraction) {
//     if (!g_synced || !ntp_seconds || !ntp_fraction) return false;

//     uint64_t now_us = time_us_64();
//     uint64_t delta_us = now_us - g_base_us;

//     uint64_t now_unix_s = g_base_unix_s + (delta_us / 1000000ULL);
//     uint64_t frac_us = delta_us % 1000000ULL;

//     uint64_t now_ntp_s = now_unix_s + UNIX_TO_NTP;

//     // Convert fractional microseconds to NTP fraction (2^32 units per second)
//     uint64_t frac = (frac_us << 32) / 1000000ULL;

//     *ntp_seconds  = (uint32_t)now_ntp_s;
//     *ntp_fraction = (uint32_t)frac;
//     return true;
// }
