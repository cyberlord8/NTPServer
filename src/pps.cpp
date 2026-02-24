#include "pps.h"

#include "hardware/gpio.h"
#include "hardware/timer.h"
#include <cstdio>

static uint32_t g_pps_gpio = 16;

static volatile uint32_t g_pps_edges = 0;
static volatile uint32_t g_pps_last_interval_us = 0;

// NEW: last edge absolute time (microseconds since boot)
static volatile uint64_t g_pps_last_edge_us = 0;

static void pps_irq_callback(uint gpio, uint32_t events)
{
    (void)events;
    if (gpio != g_pps_gpio) return;

    const uint64_t now_us_64 = time_us_64();

    // 32-bit interval is fine (wrap-safe subtraction)
    static uint32_t last_us_32 = 0;
    const uint32_t now_us_32 = (uint32_t)now_us_64;
    const uint32_t dt = now_us_32 - last_us_32;
    last_us_32 = now_us_32;

    g_pps_last_interval_us = dt;
    g_pps_last_edge_us = now_us_64;   // NEW
    g_pps_edges++;
}

void pps_init(uint32_t gpio)
{
    g_pps_gpio = gpio;

    gpio_init(g_pps_gpio);
    gpio_set_dir(g_pps_gpio, GPIO_IN);
    gpio_pull_down(g_pps_gpio);

    gpio_set_irq_enabled_with_callback(g_pps_gpio, GPIO_IRQ_EDGE_RISE, true, &pps_irq_callback);

    std::printf("PPS: IRQ ARMED ON GPIO%lu (RISING EDGE)\r\n", (unsigned long)g_pps_gpio);
}

uint32_t pps_get_edges() { return g_pps_edges; }
uint32_t pps_get_last_interval_us() { return g_pps_last_interval_us; }

// NEW
uint64_t pps_get_last_edge_us() { return g_pps_last_edge_us; }


// // src/pps.cpp
// #include "pps.h"

// #include "hardware/gpio.h"
// #include "hardware/timer.h"
// #include <cstdio>

// static uint g_pps_gpio = 16;

// static volatile uint32_t g_pps_edges = 0;
// static volatile uint32_t g_pps_last_interval_us = 0;

// static void pps_irq_callback(uint gpio, uint32_t events)
// {
//     (void)events;
//     if (gpio != g_pps_gpio) return;

//     // 32-bit microsecond timestamp is fine (wrap ~71 minutes), subtraction is wrap-safe.
//     static uint32_t last_us_32 = 0;
//     const uint32_t now_us_32 = (uint32_t)time_us_64();
//     const uint32_t dt = now_us_32 - last_us_32;
//     last_us_32 = now_us_32;

//     g_pps_last_interval_us = dt;
//     g_pps_edges++;
// }

// void pps_init(uint32_t gpio)
// {
//     g_pps_gpio = gpio;

//     gpio_init(g_pps_gpio);
//     gpio_set_dir(g_pps_gpio, GPIO_IN);

//     // Helps avoid floating if PPS isn't actually routed/active.
//     gpio_pull_down(g_pps_gpio);

//     gpio_set_irq_enabled_with_callback(g_pps_gpio, GPIO_IRQ_EDGE_RISE, true, &pps_irq_callback);

//     std::printf("PPS: IRQ ARMED ON GPIO%u (RISING EDGE)\r\n", (unsigned)g_pps_gpio);
// }

// uint32_t pps_get_edges()
// {
//     return g_pps_edges;
// }

// uint32_t pps_get_last_interval_us()
// {
//     return g_pps_last_interval_us;
// }