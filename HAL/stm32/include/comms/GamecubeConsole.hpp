#ifndef _COMMS_GAMECUBECONSOLE_HPP
#define _COMMS_GAMECUBECONSOLE_HPP

#include "comms/JoybusIO.hpp"
#include "comms/gc_report.hpp"

#include <stdint.h>

/*
 * STM32 (Frame1) GameCube / Wii Joybus device.  [timing unvalidated]
 *
 * Replaces the pico HAL's GamecubeConsole (joybus-pio / RP2040 PIO). The
 * single-wire bit transport lives in JoybusIO; this class adds the GameCube
 * command handling (0x00/0xFF ID, 0x41/0x42 origin, 0x40 poll).
 *
 * CALL CONTRACT: when WaitForPoll() returns OK, interrupts are left MASKED and
 * the caller must call SendReport() immediately (build the report first). That
 * keeps the poll->response turnaround inside the console's timing window.
 * SendReport() re-enables interrupts.
 *
 * HARDWARE: the Frame1 is otherwise USB-only; data_pin must be an exposed GPIO
 * wired to the GameCube data line (plus ground + controller power).
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

    GcPollStatus WaitForPoll();
    void SendReport(const gc_report_t *report);

  private:
    JoybusIO _io;
};

#endif
