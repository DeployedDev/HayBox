#ifndef _COMMS_GC_REPORT_HPP
#define _COMMS_GC_REPORT_HPP

#include <stdint.h>

/*
 * Standard GameCube controller status report - the 8-byte payload a controller
 * returns in response to a 0x40 poll.
 *
 * On the pico HAL this type comes from the joybus-pio library, which is
 * RP2040-PIO-only, so the STM32 (Frame1) port declares an equivalent here. The
 * field names match the pico's gc_report_t so HAL/stm32 GamecubeBackend.cpp can
 * reuse the pico backend's output-mapping verbatim.
 *
 * Bit layout assumes a little-endian target (ARM Cortex-M is LE): the first
 * declared bitfield member takes the least-significant bit, so the two packed
 * bytes come out exactly as the GameCube wire format expects -
 *
 *   byte0 (bit7..bit0): errS errL origin start  y x b a
 *   byte1 (bit7..bit0): 1    l    r      z      dUp dDn dRt dLt
 *   byte2..7          : stick_x stick_y cstick_x cstick_y l_analog r_analog
 *
 * Joybus transmits each byte MSB-first; the TX engine serialises these 8 bytes
 * in order, so the on-wire order is correct as long as the struct is exactly 8
 * bytes with no padding (asserted below).
 */
typedef struct __attribute__((packed)) {
    // byte 0
    uint8_t a : 1;
    uint8_t b : 1;
    uint8_t x : 1;
    uint8_t y : 1;
    uint8_t start : 1;
    uint8_t origin : 1;    // 0 in a normal poll response
    uint8_t errlatch : 1;  // 0
    uint8_t errstat : 1;   // 0

    // byte 1
    uint8_t dpad_left : 1;
    uint8_t dpad_right : 1;
    uint8_t dpad_down : 1;
    uint8_t dpad_up : 1;
    uint8_t z : 1;
    uint8_t r : 1;
    uint8_t l : 1;
    uint8_t high1 : 1;  // constant 1 ("controller present")

    // bytes 2..7
    uint8_t stick_x;
    uint8_t stick_y;
    uint8_t cstick_x;
    uint8_t cstick_y;
    uint8_t l_analog;
    uint8_t r_analog;
} gc_report_t;

static_assert(sizeof(gc_report_t) == 8, "gc_report_t must be exactly 8 bytes");

/*
 * Neutral report: both sticks centred (0x80), triggers released, the
 * controller-present bit set. Matches joybus-pio's default_gc_report.
 */
static const gc_report_t default_gc_report = {
    .a = 0, .b = 0, .x = 0, .y = 0,
    .start = 0, .origin = 0, .errlatch = 0, .errstat = 0,
    .dpad_left = 0, .dpad_right = 0, .dpad_down = 0, .dpad_up = 0,
    .z = 0, .r = 0, .l = 0, .high1 = 1,
    .stick_x = 128, .stick_y = 128,
    .cstick_x = 128, .cstick_y = 128,
    .l_analog = 0, .r_analog = 0,
};

#endif
