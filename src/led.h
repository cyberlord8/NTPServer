#pragma once
#include <cstdint>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "gps_state.h"

// Called once at boot (does NOT cyw43_arch_init)
int pico_led_init(void);

// Safe to call from thread context (main loop)
void led_service(void);

// Repeating timer callback (compute-only)
bool pulse_cb(repeating_timer_t* t);

// For Pico W: call this AFTER cyw43_arch_init() succeeds
void led_set_cyw43_ready(bool ready);

// Optionally let main set which state pointer to observe
void led_bind_state(const volatile GPSDeviceState* state_ptr);


// #pragma once

// #include "pico/stdlib.h"
// #include "pico/time.h"
// #include "gps_state.h"

// #ifndef LED_DELAY_ON_MS
// #define LED_DELAY_ON_MS 250
// #endif

// #ifndef LED_DELAY_OFF_MS
// #define LED_DELAY_OFF_MS 750
// #endif

// int  pico_led_init(void);
// void pico_set_led(bool led_on);

// bool led_pattern(GPSDeviceState g, uint32_t tick50ms);
// bool pulse_cb(repeating_timer_t* t);

// // NEW: call this from main loop
// void led_service(void);

// // NEW: desired LED state computed by timer callback
// extern volatile bool g_led_desired;
