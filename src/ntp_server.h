#pragma once
#include <cstdint>

// True when the server successfully bound UDP/123 and registered its recv callback.
extern bool n_status;

void ntp_server_init();
// void ntp_server_deinit();

// Convenience helper (optional, but nice for UI).
bool ntp_server_is_running();

