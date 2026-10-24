#include "comms/NesConsole.hpp"

NesConsole::NesConsole(uint8_t data_pin, uint8_t clock_pin, uint8_t latch_pin)
    : _sr(data_pin, clock_pin, latch_pin) {}

void NesConsole::Init() { _sr.Init(); }

void NesConsole::SendReport(const nes_report_t &report) {
    // NES serial shift order: A, B, Select, Start, Up, Down, Left, Right.
    const bool pressed[8] = {
        (bool)report.a,        (bool)report.b,
        (bool)report.select,   (bool)report.start,
        (bool)report.dpad_up,  (bool)report.dpad_down,
        (bool)report.dpad_left, (bool)report.dpad_right,
    };
    _sr.RespondToPoll(pressed, 8);
}
