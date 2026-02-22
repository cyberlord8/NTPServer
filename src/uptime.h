#pragma once

#include <cstddef>
#include <cstdint>

// Call once near startup (recommended).
void uptime_init();

// Seconds since uptime_init() (or since first use if uptime_init() wasn't called).
uint64_t uptime_seconds();

// Writes "DD:HH:MM:SS" into out (always NUL-terminated if out_len > 0).
void uptime_format(char* out, size_t out_len);

// #pragma once
// #include <cstdint>
// #include <cstddef>   // for size_t

// void     uptime_init();
// uint64_t uptime_seconds();
// void     uptime_format(char* out, size_t out_len); // "DD:HH:MM:SS"
