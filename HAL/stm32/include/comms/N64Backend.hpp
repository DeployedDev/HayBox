#ifndef _COMMS_N64BACKEND_HPP
#define _COMMS_N64BACKEND_HPP

#include "comms/N64Console.hpp"
#include "comms/n64_report.hpp"
#include "core/CommunicationBackend.hpp"

/*
 * N64 backend for the STM32 (Frame1) HAL. Output mapping is identical to
 * HAL/pico/src/comms/N64Backend.cpp; only the transport (N64Console, bit-banged
 * Joybus) differs.
 */
class N64Backend : public CommunicationBackend {
  public:
    N64Backend(
        InputState &inputs,
        InputSource **input_sources,
        size_t input_source_count,
        uint8_t data_pin
    );
    CommunicationBackendId BackendId();
    void SendReport();

  private:
    N64Console _n64;
    n64_report_t _report;
};

#endif
