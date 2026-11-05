#ifndef _COMMS_SNESBACKEND_HPP
#define _COMMS_SNESBACKEND_HPP

#include "comms/SnesConsole.hpp"
#include "comms/snes_report.hpp"
#include "core/CommunicationBackend.hpp"

/*
 * SNES backend for the STM32 (Frame1) HAL. Output mapping identical to
 * HAL/pico/src/comms/SnesBackend.cpp; only the transport (SnesConsole /
 * bit-banged shift register) differs.
 */
class SnesBackend : public CommunicationBackend {
  public:
    SnesBackend(
        InputState &inputs,
        InputSource **input_sources,
        size_t input_source_count,
        uint8_t data_pin,
        uint8_t clock_pin,
        uint8_t latch_pin
    );
    CommunicationBackendId BackendId();
    void SendReport();

  private:
    SnesConsole _snes;
    snes_report_t _report;
};

#endif
