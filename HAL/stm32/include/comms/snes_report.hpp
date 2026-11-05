#ifndef _COMMS_SNES_REPORT_HPP
#define _COMMS_SNES_REPORT_HPP

#include <stdint.h>

/*
 * SNES controller button state. Field layout here is for convenient access; the
 * SNES serial shift order is applied by SnesConsole, not by this struct.
 */
typedef struct {
    uint8_t a : 1;
    uint8_t b : 1;
    uint8_t x : 1;
    uint8_t y : 1;
    uint8_t l : 1;
    uint8_t r : 1;
    uint8_t select : 1;
    uint8_t start : 1;
    uint8_t dpad_up : 1;
    uint8_t dpad_down : 1;
    uint8_t dpad_left : 1;
    uint8_t dpad_right : 1;
} snes_report_t;

static const snes_report_t default_snes_report = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

#endif
