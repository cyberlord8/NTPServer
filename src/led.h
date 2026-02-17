#pragma once

#include "pico/stdlib.h"
#include "pico/time.h"
#include "gps_state.h"

#ifndef LED_DELAY_ON_MS
#define LED_DELAY_ON_MS 250
#endif

#ifndef LED_DELAY_OFF_MS
#define LED_DELAY_OFF_MS 750
#endif

int  pico_led_init(void);
void pico_set_led(bool led_on);

bool led_pattern(GPSDeviceState g, uint32_t tick50ms);
bool pulse_cb(repeating_timer_t* t);

// NEW: call this from main loop
void led_service(void);

// NEW: desired LED state computed by timer callback
extern volatile bool g_led_desired;
