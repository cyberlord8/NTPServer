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

// True when the server successfully bound UDP/123 and registered its recv callback.
extern bool n_status;

void ntp_server_init();
// void ntp_server_deinit();

// Convenience helper (optional, but nice for UI).
bool ntp_server_is_running();

