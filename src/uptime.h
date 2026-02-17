#pragma once
#include <cstdint>
#include <cstddef>   // for size_t

void     uptime_init();
uint64_t uptime_seconds();
void     uptime_format(char* out, size_t out_len); // "DD:HH:MM:SS"
