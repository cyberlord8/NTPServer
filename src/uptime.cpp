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
#include "uptime.h"

#include "hardware/timer.h"
#include <cstdio>

namespace {
    static uint64_t g_boot_us = 0;
    static bool     g_boot_set = false;

    static inline void ensure_boot_set() {
        if (!g_boot_set) {
            g_boot_us = time_us_64();
            g_boot_set = true;
        }
    }
} // namespace

void uptime_init() {
    g_boot_us = time_us_64();
    g_boot_set = true;
}

uint64_t uptime_seconds() {
    ensure_boot_set();
    const uint64_t now = time_us_64();
    return (now - g_boot_us) / 1000000ULL;
}

void uptime_format(char* out, size_t out_len) {
    if (!out || out_len == 0) return;

    uint64_t s = uptime_seconds();
    const uint64_t days = s / 86400ULL; s %= 86400ULL;
    const uint64_t hrs  = s / 3600ULL;  s %= 3600ULL;
    const uint64_t mins = s / 60ULL;    s %= 60ULL;

    // DD:HH:MM:SS
    std::snprintf(out, out_len, "%02llu:%02llu:%02llu:%02llu",
                  (unsigned long long)days,
                  (unsigned long long)hrs,
                  (unsigned long long)mins,
                  (unsigned long long)s);
}
