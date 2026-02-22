#include "gps_state.h"
#include "timebase.h"

volatile GPSDeviceState g_state = GPSDeviceState::Booting;
GpsStatus gps;

void parse_rmc(const char* line) {
    // $GNRMC,hhmmss.sss,A/V,....,ddmmyy,...
    // field 1: time, field 2: status, field 9: date

    if (!line) return;

    const char* f1 = nullptr; // time start
    const char* f2 = nullptr; // status start
    const char* f9 = nullptr; // date start

    int commas = 0;

    // Walk the sentence once, collect pointers to the start of fields we care about.
    for (const char* p = line; *p && *p != '*'; ++p) {
        if (*p == ',') {
            ++commas;
            if (commas == 1) f1 = p + 1;  // after first comma
            else if (commas == 2) f2 = p + 1;
            else if (commas == 9) f9 = p + 1;
        }
    }

    // Copy time/date into your gps struct (add last_rmc_date[] if you don't have it)
    if (f1) copy_field(gps.last_rmc_time, sizeof(gps.last_rmc_time), f1);
    if (f2) gps.rmc_valid = (f2[0] == 'A');
    if (f9) copy_field(gps.last_rmc_date, sizeof(gps.last_rmc_date), f9);

    // Now feed the timebase when valid and parseable.
    const char* rmc_time = gps.last_rmc_time;      // "hhmmss.sss"
    const char* rmc_date = gps.last_rmc_date;      // "ddmmyy"
    bool got_valid_utc = gps.rmc_valid;

    if (got_valid_utc) {
        int hh, mm, ss;
        int year, mon, day;

        if (parse_hhmmss(rmc_time, hh, mm, ss) &&
            parse_ddmmyy(rmc_date, year, mon, day)) {

            int64_t unix_utc = unix_seconds_utc(year, mon, day, hh, mm, ss);
            timebase_on_gps_utc_unix((uint64_t)unix_utc);
        }
    }
}

void parse_gga(const char* line) {
    // $GNGGA,time,lat,N,lon,W,fixQuality,numSats,hdop,...
    int field = 0;
    const char* p = line;
    const char* fixq = nullptr;
    const char* sats = nullptr;
    const char* hdop = nullptr;

    while (*p && *p != '*') {
        if (*p == ',') {
            field++;
            if (field == 6) fixq = p + 1;
            if (field == 7) sats = p + 1;
            if (field == 8) hdop = p + 1;
        }
        p++;
    }

    if (fixq) gps.gga_fix = (*fixq >= '1' && *fixq <= '8');

    uint32_t sats_u = 0;
    if (sats && parse_u32_field(sats, sats_u)) gps.sats = (int)sats_u;

    float hdop_f = 0.0f;
    if (hdop && parse_float_field(hdop, hdop_f)) gps.hdop = hdop_f;

    // if (sats) gps.sats = atoi(sats);
    // if (hdop) gps.hdop = (float)atof(hdop);
}

void parse_zda(const char* line) {
    // $GNZDA,hhmmss.sss,dd,mm,yyyy,...
    const char* f_time=nullptr; const char* f_dd=nullptr; const char* f_mm=nullptr; const char* f_yyyy=nullptr;
    const char* p=line; int c=0;

    while (*p && *p!='*') {
        if (*p==',') {
            c++;
            if (c==1) f_time=p+1;
            if (c==2) f_dd  =p+1;
            if (c==3) f_mm  =p+1;
            if (c==4) { f_yyyy=p+1; break; }
        }
        p++;
    }
    if (!f_time || !f_dd || !f_mm || !f_yyyy) return;

    const std::size_t lt = field_len(f_time);
    const std::size_t ld = field_len(f_dd);
    const std::size_t lm = field_len(f_mm);
    const std::size_t ly = field_len(f_yyyy);

    // Validate minimum lengths and digit content
    if (lt < 6 || !all_digits(f_time, 6)) return;     // hhmmss...
    if (ld < 2 || !all_digits(f_dd,   2)) return;     // dd
    if (lm < 2 || !all_digits(f_mm,   2)) return;     // mm
    if (ly < 4 || !all_digits(f_yyyy, 4)) return;     // yyyy

    // Optional: range checks (cheap, prevents nonsense)
    const int hh_i = (f_time[0]-'0')*10 + (f_time[1]-'0');
    const int mm_i = (f_time[2]-'0')*10 + (f_time[3]-'0');
    const int ss_i = (f_time[4]-'0')*10 + (f_time[5]-'0');
    const int dd_i = (f_dd[0]-'0')*10   + (f_dd[1]-'0');
    const int mo_i = (f_mm[0]-'0')*10   + (f_mm[1]-'0');

    if (hh_i < 0 || hh_i > 23) return;
    if (mm_i < 0 || mm_i > 59) return;
    if (ss_i < 0 || ss_i > 60) return;   // allow leap second
    if (mo_i < 1 || mo_i > 12) return;
    if (dd_i < 1 || dd_i > 31) return;

    char hh[3] = { f_time[0], f_time[1], 0 };
    char mm[3] = { f_time[2], f_time[3], 0 };
    char ss[3] = { f_time[4], f_time[5], 0 };

    char dd[3] = { f_dd[0],   f_dd[1],   0 };
    char mo[3] = { f_mm[0],   f_mm[1],   0 };

    char yyyy[5] = { f_yyyy[0], f_yyyy[1], f_yyyy[2], f_yyyy[3], 0 };

    snprintf(gps.last_zda, sizeof(gps.last_zda), "%s-%s-%s %s:%s:%sZ",
             yyyy, mo, dd, hh, mm, ss);
}

void update_from_nmea(const char* line) {
    // printf("%s", line);
    if (starts_with(line, "$GNRMC,") || starts_with(line, "$GPRMC,")) parse_rmc(line);
    else if (starts_with(line, "$GNGGA,") || starts_with(line, "$GPGGA,")) parse_gga(line);
    else if (starts_with(line, "$GNZDA,") || starts_with(line, "$GPZDA,")) parse_zda(line);

    // your current “no PPS yet” notion of acquired:
    g_state = (gps.rmc_valid && gps.gga_fix) ? GPSDeviceState::Acquired : GPSDeviceState::Acquiring;
}