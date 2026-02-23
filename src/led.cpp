#include "led.h"

#ifdef CYW43_WL_GPIO_LED_PIN
#include "pico/cyw43_arch.h"
#endif

// Bindable state source (avoids reaching into other modules)
static const volatile GPSDeviceState* s_state = nullptr;

// ISR->thread handoff (single-writer ISR, single-reader thread)
static volatile uint32_t s_led_desired = 0;

// For Pico W: only touch cyw43 GPIO after init
static volatile uint32_t s_cyw43_ready = 0;

// Cache last applied to avoid redundant gpio_put calls
static bool s_last_applied = false;

void led_bind_state(const volatile GPSDeviceState* state_ptr) {
    s_state = state_ptr;
}

void led_set_cyw43_ready(bool ready) {
    s_cyw43_ready = ready ? 1u : 0u;
}

int pico_led_init(void) {
#if defined(PICO_DEFAULT_LED_PIN)
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    return PICO_OK;
#elif defined(CYW43_WL_GPIO_LED_PIN)
    // Do not init cyw43 here (must be done exactly once elsewhere)
    return PICO_OK;
#else
    return PICO_ERROR_GENERIC;
#endif
}

static void pico_set_led(bool led_on) {
#if defined(PICO_DEFAULT_LED_PIN)
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
#elif defined(CYW43_WL_GPIO_LED_PIN)
    if (!s_cyw43_ready) return; // gate until cyw43 is initialized
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
#else
    (void)led_on;
#endif
}

static bool led_pattern(GPSDeviceState g, uint32_t tick50ms) {
    switch (g) {
        case GPSDeviceState::Error:
            return true; // solid on

        case GPSDeviceState::Booting:
            return (tick50ms % 6) < 3;   // ~4 Hz

        case GPSDeviceState::Acquiring:
            return (tick50ms % 10) < 5;  // ~2 Hz

        case GPSDeviceState::Acquired:
            return (tick50ms % 20) < 2;  // short pulse 1 Hz

        case GPSDeviceState::Locked: {
            uint32_t t = tick50ms % 40;  // 2 seconds
            if (t < 2) return true;
            if (t < 4) return false;
            if (t < 6) return true;
            return false;
        }
    }
    return false;
}

bool pulse_cb(repeating_timer_t* t) {
    static uint32_t tick50ms = 0;
    ++tick50ms;

    // Prefer bound pointer; fallback to user_data if you want
    GPSDeviceState st = GPSDeviceState::Error;
    if (s_state) {
        st = *s_state;
    } else if (t && t->user_data) {
        st = *static_cast<const volatile GPSDeviceState*>(t->user_data);
    }

    s_led_desired = led_pattern(st, tick50ms) ? 1u : 0u;
    return true;
}

void led_service(void) {
#if defined(CYW43_WL_GPIO_LED_PIN)
    static bool last = false;
    const bool now = s_led_desired;

    if (now != last) {
        // Optional: lock for safety; keeps CYW43 accesses serialized.
        // (Safe even if not strictly required.)
        cyw43_arch_lwip_begin();
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, now);
        cyw43_arch_lwip_end();

        last = now;
    }
#else
    pico_set_led(g_led_desired);
#endif
}

// void led_service(void) {
//     const bool desired = (s_led_desired != 0u);
//     if (desired == s_last_applied) return;
//     s_last_applied = desired;
//     pico_set_led(desired);
// }








// #include "led.h"

// // Pico W LED access uses the CYW43 driver GPIO.
// // Keep this include OUT of led.h to avoid dragging lwIP headers into everything.
// #ifdef CYW43_WL_GPIO_LED_PIN
// #include "pico/cyw43_arch.h"
// #endif

// volatile bool g_led_desired = false;

// // Perform initialization
// int pico_led_init(void) {
// #if defined(PICO_DEFAULT_LED_PIN)
//     gpio_init(PICO_DEFAULT_LED_PIN);
//     gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
//     return PICO_OK;
// #elif defined(CYW43_WL_GPIO_LED_PIN)
//     // IMPORTANT: Do NOT call cyw43_arch_init() here.
//     // It must be called exactly once by your Wi-Fi module / main.
//     return PICO_OK;
// #endif
//     return PICO_ERROR_GENERIC;
// }

// // Turn the LED on/off
// void pico_set_led(bool led_on) {
// #if defined(PICO_DEFAULT_LED_PIN)
//     gpio_put(PICO_DEFAULT_LED_PIN, led_on);
// #elif defined(CYW43_WL_GPIO_LED_PIN)
//     cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
// #endif
// }

// bool led_pattern(GPSDeviceState g, uint32_t tick50ms) {
//     switch (g) {
//         case GPSDeviceState::Error:
//             return true; // solid on

//         case GPSDeviceState::Booting:
//             return (tick50ms % 6) < 3; // ~4 Hz

//         case GPSDeviceState::Acquiring:
//             return (tick50ms % 10) < 5; // ~2 Hz

//         case GPSDeviceState::Acquired:
//             return (tick50ms % 20) < 2; // short pulse 1 Hz

//         case GPSDeviceState::Locked: {
//             uint32_t t = tick50ms % 40; // 2 seconds
//             if (t < 2) return true;
//             if (t < 4) return false;
//             if (t < 6) return true;
//             return false;
//         }
//     }
//     return false;
// }

// // IRQ context: compute-only (NO cyw43 calls here)
// bool pulse_cb(repeating_timer_t* /*t*/) {
//     static uint32_t tick50ms = 0;
//     ++tick50ms;

//     g_led_desired = led_pattern(g_state, tick50ms);
//     return true;
// }

// // Thread context: safe place to touch cyw43 gpio
// void led_service(void) {
//     pico_set_led(g_led_desired);
// }


