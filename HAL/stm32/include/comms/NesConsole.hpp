#ifndef _COMMS_NESCONSOLE_HPP
#define _COMMS_NESCONSOLE_HPP

#include "comms/ShiftRegisterIO.hpp"
#include "comms/nes_report.hpp"

#include <stdint.h>

/*
 * STM32 (Frame1) NES controller device. Wraps the shared ShiftRegisterIO and
 * applies the NES serial shift order. Replaces the pico's nes-pio NesConsole.
 */
class NesConsole {
  public:
    NesConsole(uint8_t data_pin, uint8_t clock_pin, uint8_t latch_pin);
    void Init();
    void SendReport(const nes_report_t &report);

  private:
    ShiftRegisterIO _sr;
};

#endif
