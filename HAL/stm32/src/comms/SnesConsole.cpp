#include "comms/SnesConsole.hpp"

SnesConsole::SnesConsole(uint8_t data_pin, uint8_t clock_pin, uint8_t latch_pin)
    : _sr(data_pin, clock_pin, latch_pin) {}

void SnesConsole::Init() { _sr.Init(); }

void SnesConsole::SendReport(const snes_report_t &report) {
    // SNES serial shift order: B, Y, Select, Start, Up, Down, Left, Right,
    // A, X, L, R, then 4 unused bits (not connected -> not pressed).
    const bool pressed[16] = {
        (bool)report.b,         (bool)report.y,
        (bool)report.select,    (bool)report.start,
        (bool)report.dpad_up,   (bool)report.dpad_down,
        (bool)report.dpad_left,  (bool)report.dpad_right,
        (bool)report.a,         (bool)report.x,
        (bool)report.l,         (bool)report.r,
        false, false, false, false,
    };
    _sr.RespondToPoll(pressed, 16);
}
