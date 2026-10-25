#ifndef _COMMS_GAMECUBECONSOLE_HPP
#define _COMMS_GAMECUBECONSOLE_HPP

#include "comms/JoybusIO.hpp"
#include "comms/gc_report.hpp"

#include <stdint.h>

/*
 * STM32 (Frame1) GameCube / Wii Joybus device.  [not yet tested on hardware]
 *
 * Replaces the pico HAL's GamecubeConsole (joybus-pio / RP2040 PIO). The
 * single-wire bit transport lives in JoybusIO; this class adds the GameCube
 * command handling (0x00/0xFF ID, 0x41/0x42 origin, 0x40 poll).
 *
 * CALL CONTRACT (same shape as joybus-pio): WaitForPollStart() returns OK as
 * soon as a poll's first byte (0x40) arrives, with interrupts left MASKED. The
 * caller reads inputs and builds the report while the rest of the poll is
 * still being sent, then calls WaitForPollEnd() (waits for the poll to finish)
 * and SendReport(), which replies and re-enables interrupts.
 *
 * HARDWARE: on the Frame1 the data line is PD3, carried on the USB-C port, so
 * a USB-C to GameCube cable connects it with no wiring (see config.cpp).
 */

enum class GcPollStatus { OK, ERROR };

class GamecubeConsole {
  public:
    explicit GamecubeConsole(uint8_t data_pin);

    void Init();
    bool Detect();  // true if a GameCube poll (0x40) is seen on the line

    // Active detection for the data-pin scan; mirror of N64Console::DetectActive.
    // Answers identify (0x00/0xFF) and origin (0x41/0x42); returns 1 when a real
    // poll (0x40) arrives (this IS the data pin), 0 on non-GameCube Joybus
    // traffic, -1 if the pin was idle. Runs at most budget_ms.
    int DetectActive(uint32_t budget_ms);

    GcPollStatus WaitForPollStart();
    GcPollStatus WaitForPollEnd();
    void SendReport(const gc_report_t *report);

  private:
    JoybusIO _io;
};

#endif
