#pragma once
#include <cstddef>
#include <cstdint>

class GpsUart {
public:
    static void init(uint32_t baud = 9600, uint32_t rx_gpio = 1, uint32_t tx_gpio = 0);
    static bool get_line(char* out, size_t out_cap);

private:
    static inline constexpr uint32_t RB_SIZE = 2048;
    static inline constexpr uint32_t RB_MASK = RB_SIZE - 1;

    static_assert((RB_SIZE & RB_MASK) == 0, "RB_SIZE must be power of two");

    static inline volatile uint32_t head = 0;
    static inline volatile uint32_t tail = 0;
    static inline volatile uint32_t rb_overflow_count = 0;
    static inline uint8_t rb[RB_SIZE];

    static void on_uart_rx();


};
