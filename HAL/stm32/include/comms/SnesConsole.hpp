#ifndef _COMMS_SNESCONSOLE_HPP
#define _COMMS_SNESCONSOLE_HPP

#include "comms/ShiftRegisterIO.hpp"
#include "comms/snes_report.hpp"

#include <stdint.h>

/*
 * STM32 (Frame1) SNES controller device. Wraps the shared ShiftRegisterIO and
 * applies the 16-bit SNES serial shift order. Replaces the pico's nes-pio
 * SnesConsole.
 */
class SnesConsole {
  public:
    SnesConsole(uint8_t data_pin, uint8_t clock_pin, uint8_t latch_pin);
    void Init();
    void SendReport(const snes_report_t &report);

  private:
    ShiftRegisterIO _sr;
};

#endif
