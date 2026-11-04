#ifndef _COMMS_N64_REPORT_HPP
#define _COMMS_N64_REPORT_HPP

#include <stdint.h>

/*
 * Standard N64 controller status report - the 4-byte payload returned to a 0x01
 * poll. Portable stand-in for joybus-pio's n64_report_t (RP2040-only); field
 * names match the pico's so HAL/stm32 N64Backend.cpp can reuse its mapping.
 *
 * Little-endian bitfield layout (first member = LSB) producing the N64 wire
 * bytes:
 *   byte0 (bit7..0): A B Z Start Dup Ddown Dleft Dright
 *   byte1 (bit7..0): 0 0 L R Cup Cdown Cleft Cright
 *   byte2 : stick_x (signed)   byte3 : stick_y (signed)
 */
typedef struct __attribute__((packed)) {
    // byte 0
    uint8_t dpad_right : 1;
    uint8_t dpad_left : 1;
    uint8_t dpad_down : 1;
    uint8_t dpad_up : 1;
    uint8_t start : 1;
    uint8_t z : 1;
    uint8_t b : 1;
    uint8_t a : 1;

    // byte 1
    uint8_t c_right : 1;
    uint8_t c_left : 1;
    uint8_t c_down : 1;
    uint8_t c_up : 1;
    uint8_t r : 1;
    uint8_t l : 1;
    uint8_t reserved2 : 1;  // 0
    uint8_t reserved1 : 1;  // 0 (reset flag, unused here)

    // bytes 2..3 (N64 sticks are signed, roughly -84..84)
    int8_t stick_x;
    int8_t stick_y;
} n64_report_t;

static_assert(sizeof(n64_report_t) == 4, "n64_report_t must be exactly 4 bytes");

static const n64_report_t default_n64_report = {
    .dpad_right = 0, .dpad_left = 0, .dpad_down = 0, .dpad_up = 0,
    .start = 0, .z = 0, .b = 0, .a = 0,
    .c_right = 0, .c_left = 0, .c_down = 0, .c_up = 0,
    .r = 0, .l = 0, .reserved2 = 0, .reserved1 = 0,
    .stick_x = 0, .stick_y = 0,
};

#endif
