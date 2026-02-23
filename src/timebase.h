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

// Call once at boot
void timebase_init(void);

// Feed UTC Unix seconds when you have valid RMC/ZDA time+date
void timebase_on_gps_utc_unix(uint64_t unix_utc_seconds);

// Optional: if you want to explicitly clear time validity
void timebase_clear(void);

// True when we have a valid UTC time baseline (not necessarily PPS-disciplined)
bool timebase_have_time(void);

// Your existing concept: "synced" (you can refine later to mean GPS Locked+PPS)
bool timebase_is_synced(void);

// Get "now" as NTP timestamp (seconds since 1900 + 32-bit fraction).
// Returns false if time is not available.
bool timebase_now_ntp(uint32_t* ntp_seconds, uint32_t* ntp_fraction);

// Get Unix seconds+usec for debugging/UI (optional)
bool timebase_now_unix(uint64_t* unix_seconds, uint32_t* usec);

