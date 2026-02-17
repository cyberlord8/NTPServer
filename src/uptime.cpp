#include "uptime.h"
#include "hardware/timer.h"
#include <cstdio>

static uint64_t boot_us = 0;

void uptime_init() {
    boot_us = time_us_64();
}

uint64_t uptime_seconds() {
    uint64_t now = time_us_64();
    return (boot_us == 0) ? 0 : (now - boot_us) / 1000000ULL;
}

void uptime_format(char* out, size_t out_len) {
    uint64_t s = uptime_seconds();
    uint64_t days = s / 86400ULL; s %= 86400ULL;
    uint64_t hrs  = s / 3600ULL;  s %= 3600ULL;
    uint64_t mins = s / 60ULL;    s %= 60ULL;

    // DD:HH:MM:SS
    std::snprintf(out, out_len, "%02llu:%02llu:%02llu:%02llu",
                  (unsigned long long)days,
                  (unsigned long long)hrs,
                  (unsigned long long)mins,
                  (unsigned long long)s);
}
