#ifndef _COMMS_NES_REPORT_HPP
#define _COMMS_NES_REPORT_HPP

#include <stdint.h>

/*
 * NES controller button state. Field layout here is for convenient access; the
 * NES serial shift order is applied by NesConsole, not by this struct.
 */
typedef struct {
    uint8_t a : 1;
    uint8_t b : 1;
    uint8_t select : 1;
    uint8_t start : 1;
    uint8_t dpad_up : 1;
    uint8_t dpad_down : 1;
    uint8_t dpad_left : 1;
    uint8_t dpad_right : 1;
} nes_report_t;

static const nes_report_t default_nes_report = { 0, 0, 0, 0, 0, 0, 0, 0 };

#endif
