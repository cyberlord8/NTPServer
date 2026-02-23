#include "gps_uart.h"
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"
#include <cstdio>

// ---- Static storage definitions (exactly once) ----
volatile uint32_t GpsUart::head = 0;
volatile uint32_t GpsUart::tail = 0;
volatile uint32_t GpsUart::rb_overflow_count = 0;
uint8_t GpsUart::rb[GpsUart::RB_SIZE] = {0};
// ---------------------------------------------------

void GpsUart::init(uint32_t baud, uint32_t rx_gpio, uint32_t tx_gpio) {
    // Reset ring buffer state
    head = 0;
    tail = 0;
    rb_overflow_count = 0;

    printf("Initializing GPS UART...\r\n");

    uart_init(uart0, baud);

    gpio_set_function(tx_gpio, GPIO_FUNC_UART);
    gpio_set_function(rx_gpio, GPIO_FUNC_UART);

    uart_set_format(uart0, 8, 1, UART_PARITY_NONE);
    uart_set_hw_flow(uart0, false, false);
    uart_set_fifo_enabled(uart0, true);

    // Install IRQ handler (no lambda)
    irq_set_exclusive_handler(UART0_IRQ, &GpsUart::uart0_irq_handler);
    irq_set_enabled(UART0_IRQ, true);

    // Enable RX IRQ
    uart_set_irq_enables(uart0, true, false);
}

void GpsUart::uart0_irq_handler() {
    GpsUart::on_uart_rx();
}

void GpsUart::on_uart_rx() {
    while (uart_is_readable(uart0)) {
        uint8_t c = (uint8_t)uart_getc(uart0);
        uint32_t next = (head + 1u) & RB_MASK;
        if (next != tail) {
            rb[head] = c;
            head = next;
        } else {
            rb_overflow_count++;
        }
    }
}

bool GpsUart::get_line(char* out, size_t out_cap) {
    if (!out || out_cap < 2) return false;

    // Snapshot head so we have a stable "available bytes" boundary for this call.
    const uint32_t h = head;
    const uint32_t t0 = tail;

    if (t0 == h) return false; // empty

    uint32_t probe = t0;
    size_t len = 0;
    bool truncated = false;

    while (probe != h) {
        const uint8_t c = rb[probe];
        probe = (probe + 1u) & RB_MASK;

        if (c == '\r') continue;

        if (c == '\n') {
            // We found a complete line; now we can commit consumption.
            out[len] = '\0';
            tail = probe; // consume through '\n'
            (void)truncated; // available if you want to count/report truncations
            return true;
        }

        if (len < (out_cap - 1)) {
            out[len++] = (char)c;
        } else {
            truncated = true; // keep consuming until newline, but stop writing
        }
    }

    // No newline yet -> do not consume anything.
    return false;
}

