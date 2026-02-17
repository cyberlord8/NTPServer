
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/adc.h"
#include <cmath>

#include "temp.h"
// smoothing strength
static constexpr float TAU_S = 30.0f;     // ~30s smoothing
static constexpr uint64_t MIN_UPDATE_US = 200000; // 5 Hz (0.2s)

// EMA state
static bool inited = false;
static float ema_c = 0.0f;
static uint64_t last_update_us = 0;

void temp_init()
{
    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(4); // internal temp sensor
}

float read_temp_c()
{
    // ADC is 12-bit (0..4095), Vref assumed 3.3V unless you use a different reference
    const float conversion_factor = 3.3f / (1 << 12);
    uint16_t raw = adc_read();
    float voltage = raw * conversion_factor;

    // Datasheet approximation:
    // T = 27 - (V - 0.706) / 0.001721
    float temp_c = 27.0f - (voltage - 0.706f) / 0.001721f;
    return temp_c;
}

float temp_ema_update_throttled(float sample_c)
{
    uint64_t now = time_us_64();

    if (!inited) {
        ema_c = sample_c;
        inited = true;
        last_update_us = now;
        return ema_c;
    }

    uint64_t elapsed_us = now - last_update_us;
    if (elapsed_us < MIN_UPDATE_US) {
        // too soon; keep last EMA
        return ema_c;
    }

    float dt_s = (float)elapsed_us / 1e6f;
    last_update_us = now;

    float alpha = 1.0f - std::exp(-dt_s / TAU_S);
    ema_c += alpha * (sample_c - ema_c);
    return ema_c;
}

