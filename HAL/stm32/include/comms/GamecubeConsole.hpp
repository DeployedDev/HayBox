#ifndef _COMMS_GAMECUBECONSOLE_HPP
#define _COMMS_GAMECUBECONSOLE_HPP

#include "comms/gc_report.hpp"

#include <Arduino.h>
#include <stdint.h>

/*
 * STM32 (Frame1) GameCube / Wii Joybus device.  [DRAFT - timing unvalidated]
 *
 * Replaces the pico HAL's GamecubeConsole (JonnyHaystack/joybus-pio, built on
 * the RP2040 PIO). The STM32F303 has no PIO, so the single-wire Joybus protocol
 * is bit-banged on one GPIO with cycle-accurate timing (DWT->CYCCNT) and
 * interrupts masked only during each command/response burst.
 *
 * ELECTRICAL MODEL
 *   The GameCube data line is open-drain, idling HIGH through the console's
 *   pull-up to 3.43 V. `data_pin` is set to OUTPUT_OPEN_DRAIN: writing 0 pulls
 *   it low, writing 1 releases it (hi-Z); the input register still reads the
 *   live level, so one pin both drives and samples.
 *
 *   HARDWARE REQUIRED (the Frame1 is otherwise USB-only): data line + ground +
 *   controller power must be physically wired, and `data_pin` must be a free,
 *   exposed GPIO.
 *
 * JOYBUS BIT TIMING (bit cell = 4 us): 0 = ~3us low +1us high; 1 = ~1us low
 * +3us high; a received bit is sampled ~2us after its falling edge. The cycle
 * constants in the .cpp MUST be checked on a logic analyser on real hardware.
 *
 * CALL CONTRACT: when WaitForPoll() returns OK, interrupts are left MASKED and
 * the caller must call SendReport() immediately (no input scanning in between -
 * build the report first). SendReport() re-enables interrupts. This keeps the
 * command->response turnaround inside the console's timing window.
 */

enum class GcPollStatus { OK, ERROR };

class GamecubeConsole {
  public:
    explicit GamecubeConsole(uint8_t data_pin);

    // Configure the open-drain GPIO + enable the cycle counter. Call once.
    void Init();

    // True if a console is actively polling the data line (used by
    // detect_console() to auto-select the GameCube backend). Returns quickly.
    bool Detect();

    // Block until the console issues a poll (0x40), transparently answering the
    // 0x00/0xFF (ID) and 0x41/0x42 (origin) commands that precede it. On OK,
    // IRQs are masked and SendReport() must be called immediately (see contract
    // above). Returns ERROR on framing error / timeout (IRQs restored).
    GcPollStatus WaitForPoll();

    // Send the 8-byte status response to the poll read by WaitForPoll(), then
    // re-enable interrupts.
    void SendReport(const gc_report_t *report);

  private:
    uint8_t _pin;        // Arduino pin id (from pinout.joybus_data)
    PinName _pn;         // resolved PinName for fast IO
    uint32_t _c1, _c2, _c3;  // cycle counts for 1/2/3 us (set in Init)

    void SendBytes(const uint8_t *data, uint8_t len);
    bool ReadBytes(uint8_t *data, uint8_t len);  // false on timeout
    void SendStopBit();
};

#endif
