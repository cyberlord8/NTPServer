#include "gps_uart.h"
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"

void GpsUart::init(uint32_t baud, uint32_t rx_gpio, uint32_t tx_gpio) {
    uart_init(uart0, baud);

    // Map GPIOs to UART0 function
    gpio_set_function(tx_gpio, GPIO_FUNC_UART);
    gpio_set_function(rx_gpio, GPIO_FUNC_UART);

    // 8N1 (default), but set explicitly
    uart_set_format(uart0, 8, 1, UART_PARITY_NONE);
    uart_set_hw_flow(uart0, false, false);
    uart_set_fifo_enabled(uart0, true);

    // IRQ on RX
    irq_set_exclusive_handler(UART0_IRQ, []() { GpsUart::on_uart_rx(); });
    irq_set_enabled(UART0_IRQ, true);
    uart_set_irq_enables(uart0, true, false);
}

void GpsUart::on_uart_rx() {
    while (uart_is_readable(uart0)) {
        uint8_t c = (uint8_t)uart_getc(uart0);
        uint32_t next = (head + 1u) & RB_MASK;
        if (next != tail) {
            rb[head] = c;
            head = next;
        } else {
            // Ring buffer overflow: record it so the parser can resync
            rb_overflow_count++;
        }
    }
}

bool GpsUart::get_line(char* out, size_t out_cap) {
    static bool in_sentence = false;
    static bool discarding = false;
    static size_t idx = 0;
    static uint32_t last_overflow_seen = 0;

    // If ISR overflowed the ring buffer, resync on next '$'
    uint32_t seen = rb_overflow_count;
    if (seen != last_overflow_seen) {
        last_overflow_seen = seen;
        in_sentence = false;
        discarding = false;
        idx = 0;
    }

    while (tail != head) {
        uint8_t c = rb[tail];
        tail = (tail + 1u) & RB_MASK;

        if (!in_sentence) {
            if (c == '$') {
                in_sentence = true;
                discarding = false;
                idx = 0;
                if (out_cap > 0) out[idx++] = '$';
            }
            continue;
        }

        // Sentence in progress
        if (c == '\r') continue;

        if (c == '\n') {
            // End of sentence
            in_sentence = false;

            if (discarding) {
                // We intentionally dropped this sentence (too long / bad)
                discarding = false;
                idx = 0;
                return false;
            }

            if (out_cap > 0) {
                if (idx >= out_cap) idx = out_cap - 1;
                out[idx] = '\0';
            }

            return (idx > 0);
        }

        if (discarding) {
            // Continue dropping until newline
            continue;
        }

        if (idx + 1 < out_cap) {
            out[idx++] = (char)c;
        } else {
            // Output buffer too small: drop remainder of this sentence
            discarding = true;
        }
    }

    return false;
}

