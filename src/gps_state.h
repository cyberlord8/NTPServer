#pragma once
#include <cstdint>
#include <cerrno>   // errno
#include <cstdlib>  // strtol, strtof
#include <cstddef>  // std::size_t
#include <cstdio>   // snprintf
#include <cstdlib>  // atoi, atof
#include <cctype>

enum class GPSDeviceState : uint8_t 
{ 
    Error=0, 
    Booting, 
    Acquiring, 
    Acquired, 
    Locked 
};


// Header-only helper for display/logging.
inline const char* state_str(GPSDeviceState s) {
    switch (s) {
        case GPSDeviceState::Error:     return "ERROR";
        case GPSDeviceState::Booting:   return "BOOTING";
        case GPSDeviceState::Acquiring: return "ACQUIRING";
        case GPSDeviceState::Acquired:  return "ACQUIRED";
        case GPSDeviceState::Locked:    return "LOCKED";
    }
    return "?";
}


struct GpsStatus {
    bool rmc_valid = false;
    bool gga_fix = false;
    int  sats = -1;
    float hdop = -1.0f;
    char last_rmc_time[16] = ""; // hhmmss.sss
    char last_rmc_date[16] = ""; // ddmmyy
    char last_zda[32] = "";      // dd-mm-yyyy hh:mm:ss
};

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

// Very lightweight parsing (enough for display)
static inline bool starts_with(const char* s, const char* p) {
    while (*p) if (*s++ != *p++) return false;
    return true;
}

static inline std::size_t field_len(const char* f) {
    std::size_t n = 0;
    while (f[n] && f[n] != ',' && f[n] != '*' && f[n] != '\r' && f[n] != '\n') n++;
    return n;
}
static inline bool is_digit(char c) { return c >= '0' && c <= '9'; }
static inline bool all_digits(const char* s, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) if (!is_digit(s[i])) return false;
    return true;
}

static inline bool parse_u32_field(const char* f, uint32_t& out) {
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

static inline bool parse_float_field(const char* f, float& out) {
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


// --- helpers ------------------------------------------------------------

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

void parse_rmc(const char* line);
void parse_gga(const char* line);
void parse_zda(const char* line);
void update_from_nmea(const char* line);


extern volatile GPSDeviceState g_state;
extern GpsStatus gps;


