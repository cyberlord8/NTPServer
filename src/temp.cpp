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
#include "temp.h"

#include "hardware/adc.h"
#include "pico/time.h"

#include <cstdint>
#include <cmath>

// Module-local configuration/state (no globals exported)
namespace {
    // Smoothing strength
    constexpr float    TAU_S          = 30.0f;     // ~30s smoothing
    constexpr uint64_t MIN_UPDATE_US  = 200000u;   // 5 Hz (0.2s)

    // ADC conversion
    constexpr float    ADC_VREF_V     = 3.3f;      // default Pico Vref assumption
    constexpr uint32_t ADC_COUNTS     = (1u << 12); // 12-bit ADC => 4096 counts

    struct EmaState {
        bool     inited         = false;
        float    ema_c          = 0.0f;
        uint64_t last_update_us = 0;
    };

    EmaState g_ema{};
} // namespace

void temp_init()
{
    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(4); // internal temp sensor channel
}

float read_temp_c()
{
    // ADC is 12-bit (0..4095). Vref assumed 3.3V unless you use a different reference.
    const float conversion_factor = ADC_VREF_V / static_cast<float>(ADC_COUNTS);

    const uint16_t raw = adc_read();
    const float voltage = static_cast<float>(raw) * conversion_factor;

    // RP2040 datasheet approximation:
    // T = 27 - (V - 0.706) / 0.001721
    const float temp_c = 27.0f - (voltage - 0.706f) / 0.001721f;
    return temp_c;
}

float temp_ema_update_throttled(float sample_c)
{
    const uint64_t now = time_us_64();

    if (!g_ema.inited) {
        g_ema.ema_c = sample_c;
        g_ema.inited = true;
        g_ema.last_update_us = now;
        return g_ema.ema_c;
    }

    const uint64_t elapsed_us = now - g_ema.last_update_us;
    if (elapsed_us < MIN_UPDATE_US) {
        // Too soon; keep last EMA
        return g_ema.ema_c;
    }

    g_ema.last_update_us = now;

    const float dt_s = static_cast<float>(elapsed_us) * 1e-6f;

    // alpha = 1 - e^(-dt/tau)
    const float alpha = 1.0f - ::expf(-dt_s / TAU_S);

    g_ema.ema_c += alpha * (sample_c - g_ema.ema_c);
    return g_ema.ema_c;
}

