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
#include "gps_state.h"
#include "timebase.h"
#include "pps.h"
#include "hardware/timer.h"

volatile GPSDeviceState g_state = GPSDeviceState::Booting;
GpsStatus gps;

static bool pps_recent_and_1hz()
{
    // Need at least 2 edges so the interval is real.
    const uint32_t edges = pps_get_edges();
    if (edges < 2) return false;

    // Interval sanity check (~1 Hz).
    const uint32_t interval_us = pps_get_last_interval_us();
    if (interval_us < 900000u || interval_us > 1100000u) return false;

    // "Recent" check (PPS still present).
    const uint64_t last_edge_us = pps_get_last_edge_us();
    if (last_edge_us == 0) return false;

    const uint64_t age_us = time_us_64() - last_edge_us;
    if (age_us > 1500000ULL) return false;

    return true;
}

// Very lightweight parsing (enough for display)
static bool starts_with(const char* s, const char* p) {
    while (*p) if (*s++ != *p++) return false;
    return true;
}

static std::size_t field_len(const char* f) {
    std::size_t n = 0;
    while (f[n] && f[n] != ',' && f[n] != '*' && f[n] != '\r' && f[n] != '\n') n++;
    return n;
}
static bool is_digit(char c) { return c >= '0' && c <= '9'; }
static bool all_digits(const char* s, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) if (!is_digit(s[i])) return false;
    return true;
}

static bool parse_u32_field(const char* f, uint32_t& out) {
    const std::size_t n = field_len(f);
    if (n == 0) return false;

    // Copy field into a small temp buffer so parsing can't run past the comma.
    // (NMEA numeric fields are small; 16 is plenty for sats/fixq/hdop.)
    char buf[16];
    if (n >= sizeof(buf)) return false;

    for (std::size_t i = 0; i < n; ++i) buf[i] = f[i];
    buf[n] = '\0';

    char* end = nullptr;
    errno = 0;
    long v = std::strtol(buf, &end, 10);

    if (errno != 0 || end == buf || *end != '\0' || v < 0) return false;
    out = (uint32_t)v;
    return true;
}

static bool parse_float_field(const char* f, float& out) {
    const std::size_t n = field_len(f);
    if (n == 0) return false;

    char buf[16];
    if (n >= sizeof(buf)) return false;

    for (std::size_t i = 0; i < n; ++i) buf[i] = f[i];
    buf[n] = '\0';

    char* end = nullptr;
    errno = 0;
    float v = std::strtof(buf, &end);

    if (errno != 0 || end == buf || *end != '\0') return false;
    out = v;
    return true;
}

// Parses "hhmmss" or "hhmmss.sss" into h/m/s (seconds truncated)
static bool parse_hhmmss(const char* s, int& hh, int& mm, int& ss) {
    if (!s) return false;
    // Need at least 6 digits
    for (int i = 0; i < 6; ++i) if (!std::isdigit((unsigned char)s[i])) return false;

    hh = (s[0]-'0')*10 + (s[1]-'0');
    mm = (s[2]-'0')*10 + (s[3]-'0');
    ss = (s[4]-'0')*10 + (s[5]-'0');

    if (hh < 0 || hh > 23) return false;
    if (mm < 0 || mm > 59) return false;
    if (ss < 0 || ss > 60) return false; // allow leap second "60"
    return true;
}

// Parses "ddmmyy" into Y/M/D (assumes 2000-2099 for yy 00-99)
static bool parse_ddmmyy(const char* s, int& year, int& month, int& day) {
    if (!s) return false;
    for (int i = 0; i < 6; ++i) if (!std::isdigit((unsigned char)s[i])) return false;

    day   = (s[0]-'0')*10 + (s[1]-'0');
    month = (s[2]-'0')*10 + (s[3]-'0');
    int yy = (s[4]-'0')*10 + (s[5]-'0');

    // Common GPS assumption: 2000..2099
    year = 2000 + yy;

    if (month < 1 || month > 12) return false;
    if (day < 1 || day > 31) return false;
    return true;
}

static void copy_field(char* dst, size_t dst_size, const char* start) {
    if (!dst || dst_size == 0) return;
    if (!start) { dst[0] = '\0'; return; }

    size_t i = 0;
    while (start[i] && start[i] != ',' && start[i] != '*' && i < (dst_size - 1)) {
        dst[i] = start[i];
        ++i;
    }
    dst[i] = '\0';
}


// Howard Hinnant's "days from civil" algorithm (public domain style)
// Returns days since 1970-01-01.
static int64_t days_from_civil(int y, unsigned m, unsigned d) {
    y -= (m <= 2);
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);                      // [0, 399]
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;  // [0, 365]
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;          // [0, 146096]
    return (int64_t)(era * 146097 + (int)doe - 719468);
}

static int64_t unix_seconds_utc(int year, int month, int day,
                                int hour, int minute, int second) {
    int64_t days = days_from_civil(year, (unsigned)month, (unsigned)day);
    return days * 86400 + hour * 3600 + minute * 60 + second;
}

void parse_rmc(const char* line) {
    // $GNRMC,hhmmss.sss,A/V,....,ddmmyy,...
    // field 1: time, field 2: status, field 9: date

    if (!line) return;

    const char* f1 = nullptr;  // time start
    const char* f2 = nullptr;  // status start
    const char* f9 = nullptr;  // date start

    const char* e1 = nullptr;  // time end (comma or '*')
    const char* e2 = nullptr;  // status end
    const char* e9 = nullptr;  // date end

    int field = 0; // 0 = talker+type, 1=time, 2=status, ...

    // Field 0 begins at line[0]; field N begins right after the (N)th comma.
    // We care about starts for 1,2,9 and their ends (next comma or '*').
    for (const char* p = line; *p; ++p) {
        if (*p == '*') {
            // Sentence payload ends here; close any open field ends we care about.
            if (f1 && !e1) e1 = p;
            if (f2 && !e2) e2 = p;
            if (f9 && !e9) e9 = p;
            break;
        }

        if (*p == ',') {
            // We just ended "field".
            if (field == 1 && f1 && !e1) e1 = p;
            else if (field == 2 && f2 && !e2) e2 = p;
            else if (field == 9 && f9 && !e9) e9 = p;

            ++field;

            // Next field starts after this comma (may be empty).
            const char* start = p + 1;
            if (field == 1) f1 = start;
            else if (field == 2) f2 = start;
            else if (field == 9) f9 = start;
        }
    }

    // If line ended without '*', close any still-open ends.
    if (f1 && !e1) {
        const char* p = f1;
        while (*p && *p != ',' && *p != '*') ++p;
        e1 = p;
    }
    if (f2 && !e2) {
        const char* p = f2;
        while (*p && *p != ',' && *p != '*') ++p;
        e2 = p;
    }
    if (f9 && !e9) {
        const char* p = f9;
        while (*p && *p != ',' && *p != '*') ++p;
        e9 = p;
    }

    auto field_nonempty = [](const char* s, const char* e) -> bool {
        return s && e && (e > s) && (*s != '\0');
    };

    // Preserve raw strings for UI/status (optional but matches your current behavior).
    if (field_nonempty(f1, e1)) copy_field(gps.last_rmc_time, sizeof(gps.last_rmc_time), f1);
    if (field_nonempty(f9, e9)) copy_field(gps.last_rmc_date, sizeof(gps.last_rmc_date), f9);

    // Status: only set if present; otherwise leave as-is (or set false if you prefer).
    if (field_nonempty(f2, e2)) {
        gps.rmc_valid = (f2[0] == 'A');
    }

    // Feed timebase only when status is 'A' and time+date exist.
    if (!gps.rmc_valid) return;
    if (!field_nonempty(f1, e1) || !field_nonempty(f9, e9)) return;

    int hh, mm, ss;
    int year, mon, day;

    // parse_* operate on NUL-terminated strings, so use gps.* buffers (already copied)
    // OR, if you want, add parse routines that accept (start,end).
    if (parse_hhmmss(gps.last_rmc_time, hh, mm, ss) &&
        parse_ddmmyy(gps.last_rmc_date, year, mon, day)) {

        const int64_t unix_utc = unix_seconds_utc(year, mon, day, hh, mm, ss);
        timebase_on_gps_utc_unix((uint64_t)unix_utc);
    }
}

void parse_gga(const char* line) {
    // $GNGGA,time,lat,N,lon,W,fixQuality,numSats,hdop,...
    if (!line) return;

    const char* fixq = nullptr;
    const char* sats = nullptr;
    const char* hdop = nullptr;

    const char* fixq_end = nullptr;
    const char* sats_end = nullptr;
    const char* hdop_end = nullptr;

    int field = 0; // 0 = talker+type, 1=time, 2=lat, 3=N/S, 4=lon, 5=E/W, 6=fixq, 7=sats, 8=hdop...

    for (const char* p = line; *p; ++p) {
        if (*p == '*') {
            if (fixq && !fixq_end) fixq_end = p;
            if (sats && !sats_end) sats_end = p;
            if (hdop && !hdop_end) hdop_end = p;
            break;
        }

        if (*p == ',') {
            // close the field we just finished
            if (field == 6 && fixq && !fixq_end) fixq_end = p;
            else if (field == 7 && sats && !sats_end) sats_end = p;
            else if (field == 8 && hdop && !hdop_end) hdop_end = p;

            ++field;

            // next field starts after this comma
            const char* start = p + 1;
            if (field == 6) fixq = start;
            else if (field == 7) sats = start;
            else if (field == 8) hdop = start;
        }
    }

    auto field_nonempty = [](const char* s, const char* e) -> bool {
        return s && e && (e > s) && (*s != '\0');
    };

    // Fix quality is a single digit typically: 0 = invalid, 1 = GPS fix, 2 = DGPS, etc.
    if (field_nonempty(fixq, fixq_end)) {
        const char c = fixq[0];
        gps.gga_fix = (c >= '1' && c <= '8'); // keep your existing rule
        // If you want "any nonzero is a fix" instead: gps.gga_fix = (c != '0');
    }

    if (field_nonempty(sats, sats_end)) {
        uint32_t sats_u = 0;
        if (parse_u32_field(sats, sats_u)) {
            gps.sats = (int)sats_u;
        }
    }

    if (field_nonempty(hdop, hdop_end)) {
        float hdop_f = 0.0f;
        if (parse_float_field(hdop, hdop_f)) {
            gps.hdop = hdop_f;
        }
    }
}

void parse_zda(const char* line) {
    // $GNZDA,hhmmss.sss,dd,mm,yyyy,...
    if (!line) return;

    const char* f_time = nullptr;
    const char* f_dd   = nullptr;
    const char* f_mm   = nullptr;
    const char* f_yyyy = nullptr;

    const char* e_time = nullptr;
    const char* e_dd   = nullptr;
    const char* e_mm   = nullptr;
    const char* e_yyyy = nullptr;

    int field = 0; // 0=talker+type, 1=time, 2=dd, 3=mm, 4=yyyy, ...

    for (const char* p = line; *p; ++p) {
        if (*p == '*') {
            if (f_time && !e_time) e_time = p;
            if (f_dd   && !e_dd)   e_dd   = p;
            if (f_mm   && !e_mm)   e_mm   = p;
            if (f_yyyy && !e_yyyy) e_yyyy = p;
            break;
        }

        if (*p == ',') {
            // close the field we just ended
            if (field == 1 && f_time && !e_time) e_time = p;
            else if (field == 2 && f_dd && !e_dd) e_dd = p;
            else if (field == 3 && f_mm && !e_mm) e_mm = p;
            else if (field == 4 && f_yyyy && !e_yyyy) e_yyyy = p;

            ++field;

            // next field starts after this comma
            const char* start = p + 1;
            if (field == 1) f_time = start;
            else if (field == 2) f_dd = start;
            else if (field == 3) f_mm = start;
            else if (field == 4) f_yyyy = start;
        }
    }

    auto field_nonempty = [](const char* s, const char* e) -> bool {
        return s && e && (e > s) && (*s != '\0');
    };

    if (!field_nonempty(f_time, e_time) ||
        !field_nonempty(f_dd,   e_dd)   ||
        !field_nonempty(f_mm,   e_mm)   ||
        !field_nonempty(f_yyyy, e_yyyy)) {
        return;
    }

    // Validate minimum lengths and digit content
    // NOTE: We only need the first 6 digits of time and exact digits for dd/mm/yyyy.
    if ((e_time - f_time) < 6 || !all_digits(f_time, 6)) return;  // hhmmss...
    if ((e_dd   - f_dd)   < 2 || !all_digits(f_dd,   2)) return;  // dd
    if ((e_mm   - f_mm)   < 2 || !all_digits(f_mm,   2)) return;  // mm
    if ((e_yyyy - f_yyyy) < 4 || !all_digits(f_yyyy, 4)) return;  // yyyy

    // Optional: range checks (cheap, prevents nonsense)
    const int hh_i = (f_time[0]-'0')*10 + (f_time[1]-'0');
    const int mm_i = (f_time[2]-'0')*10 + (f_time[3]-'0');
    const int ss_i = (f_time[4]-'0')*10 + (f_time[5]-'0');
    const int dd_i = (f_dd[0]-'0')*10   + (f_dd[1]-'0');
    const int mo_i = (f_mm[0]-'0')*10   + (f_mm[1]-'0');

    if (hh_i > 23) return;
    if (mm_i > 59) return;
    if (ss_i > 60) return; // allow leap second
    if (mo_i < 1 || mo_i > 12) return;
    if (dd_i < 1 || dd_i > 31) return;

    // Format: YYYY-MM-DD HH:MM:SSZ
    // Use direct chars to avoid temp buffers.
    snprintf(gps.last_zda, sizeof(gps.last_zda),
             "%c%c%c%c-%c%c-%c%c %c%c:%c%c:%c%cZ",
             f_yyyy[0], f_yyyy[1], f_yyyy[2], f_yyyy[3],
             f_mm[0],   f_mm[1],
             f_dd[0],   f_dd[1],
             f_time[0], f_time[1],
             f_time[2], f_time[3],
             f_time[4], f_time[5]);
}

void gps_state_service()
{
    // Your pre-PPS notion of acquired:
    const bool acquired = (gps.rmc_valid && gps.gga_fix);

    if (!acquired) {
        g_state = GPSDeviceState::Acquiring;
        return;
    }

    // If acquired, promote to Locked only when PPS is present and sane
    g_state = pps_recent_and_1hz() ? GPSDeviceState::Locked
                                   : GPSDeviceState::Acquired;
}

void update_from_nmea(const char* line) {
    if (!line || line[0] != '$') return;

    bool handled = false;

    // Accept any talker: $??RMC, $??GGA, $??ZDA
    if (starts_with(line + 3, "RMC,")) {
        parse_rmc(line);
        handled = true;
    } else if (starts_with(line + 3, "GGA,")) {
        parse_gga(line);
        handled = true;
    } else if (starts_with(line + 3, "ZDA,")) {
        parse_zda(line);
        handled = true;
    }

    if (!handled) return;

    gps_state_service();
    // // Current “no PPS yet” notion of acquired:
    // g_state = (gps.rmc_valid && gps.gga_fix) ? GPSDeviceState::Acquired
    //                                         : GPSDeviceState::Acquiring;
}
