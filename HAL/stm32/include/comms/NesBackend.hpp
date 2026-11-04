#ifndef _COMMS_NESBACKEND_HPP
#define _COMMS_NESBACKEND_HPP

#include "comms/NesConsole.hpp"
#include "comms/nes_report.hpp"
#include "core/CommunicationBackend.hpp"

/*
 * NES backend for the STM32 (Frame1) HAL. Output mapping identical to
 * HAL/pico/src/comms/NesBackend.cpp; only the transport (NesConsole /
 * bit-banged shift register) differs.
 */
class NesBackend : public CommunicationBackend {
  public:
    NesBackend(
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
    NesConsole _nes;
    nes_report_t _report;
};

#endif
