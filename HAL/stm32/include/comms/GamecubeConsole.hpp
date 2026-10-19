#ifndef _COMMS_GAMECUBECONSOLE_HPP
#define _COMMS_GAMECUBECONSOLE_HPP

#include "comms/gc_report.hpp"

#include <Arduino.h>
#include <stdint.h>

/*
 * STM32 (Frame1) GameCube / Wii Joybus device.  [DRAFT - timing unvalidated]
 *
 * This is the STM32 replacement for the pico HAL's GamecubeConsole, which comes
 * from JonnyHaystack/joybus-pio and is built on the RP2040 PIO. The STM32F303
 * has no PIO, so the single-wire Joybus protocol is bit-banged on one GPIO with
 * cycle-accurate timing (DWT->CYCCNT) and interrupts masked for the duration of
 * each transaction.
 *
 * ELECTRICAL MODEL
 *   The GameCube data line is open-drain, idling HIGH through the console's
 *   pull-up to 3.43 V. `data_pin` is configured as open-drain output, so
 *   writing 0 pulls the line low and writing 1 releases it (hi-Z); in OD mode
 *   the input register still reads the live line level, so the one pin both
 *   drives and samples with no direction changes.
 *
 *   HARDWARE REQUIRED (the Frame1 is otherwise USB-only): the data line, ground,
 *   and controller power (3.43 V / 5 V from the GC port, or keep the board on
 *   USB power) must be physically wired. `data_pin` must be a free GPIO - it is
 *   NOT routed on the stock board.
 *
 * JOYBUS BIT TIMING (bit cell = 4 us)
 *   logical 0 : ~3 us low + ~1 us high
 *   logical 1 : ~1 us low + ~3 us high
 *   a received bit is sampled ~2 us after its falling edge.
 *   These constants live in the .cpp and MUST be checked on a logic analyser on
 *   real hardware before trusting them.
 *
 * COMMANDS HANDLED
 *   0x00  ID/probe  -> respond 3 bytes (0x09 0x00 0x03: standard wired pad)
 *   0x41  origin    -> respond 10 bytes (neutral origins)
 *   0x40  poll      -> read 2 rumble bytes, then the caller supplies the
 *                      gc_report_t and we send the 8-byte status response.
 */

enum class GcPollStatus { OK, ERROR };

class GamecubeConsole {
  public:
    explicit GamecubeConsole(PinName data_pin);

    // Configure the open-drain GPIO and enable the cycle counter. Call once.
    void Init();

    // Returns true if a console answers the 0x00 ID command on the data line.
    // Used by detect_console() to pick the GameCube backend automatically.
    bool Detect();

    // Block until the console issues a poll (0x40), transparently answering the
    // 0x00 and 0x41 commands that precede it. Returns OK once a poll command has
    // been fully received (the status response is NOT sent yet), or ERROR on a
    // framing error / timeout.
    //
    // NOTE: this first cut reads the whole poll command before the caller builds
    // the report, which costs a little input latency versus the pico's
    // WaitForPollStart()/WaitForPollEnd() split. Matching that split (scan fast
    // inputs mid-command) is a later refinement once the base protocol works.
    GcPollStatus WaitForPoll();

    // Send the 8-byte status report in response to the poll read by WaitForPoll.
    void SendReport(const gc_report_t *report);

  private:
    PinName _pin;
    // GPIO register handles + timing constants are set up in the .cpp.
};

#endif
