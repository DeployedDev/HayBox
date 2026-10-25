#ifndef _COMMS_N64CONSOLE_HPP
#define _COMMS_N64CONSOLE_HPP

#include "comms/JoybusIO.hpp"
#include "comms/n64_report.hpp"

#include <stdint.h>

/*
 * STM32 (Frame1) N64 Joybus device.  [timing unvalidated]
 *
 * Shares the single-wire transport (JoybusIO) and data pin with GameCube; adds
 * N64 command handling (0x00/0xFF ID, 0x01 poll). The Controller Pak
 * (rumble/mempak, commands 0x02/0x03) is NOT implemented - identity reports no
 * pak, and pak reads/writes are left unanswered, as on the pico.
 *
 * Same call contract as GamecubeConsole: on WaitForPoll()==OK interrupts are
 * left masked and SendReport() must follow immediately.
 */

enum class N64PollStatus { OK, ERROR };

class N64Console {
  public:
    explicit N64Console(uint8_t data_pin);

    void Init();
    bool Detect();  // true if an N64 poll (0x01) is seen on the line

    // Active detection for the data-pin scan. Behaves as a controller on the
    // pin: answers the identify (0x00/0xFF) so a host that only polls after a
    // valid reply (Raphnet adapter, real consoles) advances to polling. Returns
    // 1 if a real poll (0x01) arrives (this IS the data pin), 0 if the pin
    // carried Joybus traffic but no N64 poll, -1 if the pin was idle. Runs at
    // most budget_ms; idle pins bail fast.
    int DetectActive(uint32_t budget_ms);

    N64PollStatus WaitForPoll();
    void SendReport(const n64_report_t *report);

  private:
    JoybusIO _io;
};

#endif
