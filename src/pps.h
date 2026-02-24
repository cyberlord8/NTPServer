// src/pps.h
#pragma once
#include <cstdint>

void     pps_init(uint32_t gpio);
uint32_t pps_get_edges();
uint32_t pps_get_last_interval_us();
uint64_t pps_get_last_edge_us();