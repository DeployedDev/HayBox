#ifndef _COMMS_GAMECUBEBACKEND_HPP
#define _COMMS_GAMECUBEBACKEND_HPP

#include "comms/GamecubeConsole.hpp"
#include "comms/gc_report.hpp"
#include "core/CommunicationBackend.hpp"

/*
 * GameCube / Wii backend for the STM32 (Frame1) HAL. The output mapping is
 * identical to HAL/pico/src/comms/GamecubeBackend.cpp; only the underlying
 * GamecubeConsole (Joybus transport) differs (bit-banged, no PIO).
 */
class GamecubeBackend : public CommunicationBackend {
  public:
    GamecubeBackend(
        InputState &inputs,
        InputSource **input_sources,
        size_t input_source_count,
        uint8_t data_pin
    );
    CommunicationBackendId BackendId();
    void SendReport();

  private:
    GamecubeConsole _gamecube;
    gc_report_t _report;
};

#endif
