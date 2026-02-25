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

void parse_rmc(const char* line);
void parse_gga(const char* line);
void parse_zda(const char* line);
void update_from_nmea(const char* line);
void gps_state_service();

extern volatile GPSDeviceState g_state;
extern GpsStatus gps;


