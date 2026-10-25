#include "comms/console_detection.hpp"

#include "comms/GamecubeConsole.hpp"
#include "comms/N64Console.hpp"
#include "core/pinout.hpp"

#include <Arduino.h>
#include <config.pb.h>

/*
 * Frame1 console auto-detection (pico-parity: type detection on known pins).
 *
 * NES/SNES: counted on the nes_* lines by latch/clock pulse ratio. GameCube/N64
 * (Joybus): probed on the known data pin pinout.joybus_data (PD3, validated on
 * hardware), exactly as the pico probes its fixed joybus pin. The probe is
 * ACTIVE: we briefly behave as a controller and answer the identify command,
 * because real consoles (and the Raphnet adapter) only start polling AFTER a
 * valid identify reply. Whichever controller the host then polls tells us the
 * console: 0x01 -> N64, 0x40 -> GameCube. Returns UNSPECIFIED (-> default USB
 * backend) when nothing is polling.
 */

#define JOYBUS_PROBE_MS 60  // active-probe window on the known data pin

static volatile uint32_t latch_pulses = 0;
static volatile uint32_t clock_pulses = 0;
static void latch_irq() { latch_pulses++; }
static void clock_irq() { clock_pulses++; }

CommunicationBackendId detect_console(const Pinout &pinout) {
    // NES / SNES: count latch vs clock pulses over a short window.
    if (pinout.nes_latch > -1 && pinout.nes_clock > -1) {
        latch_pulses = 0;
        clock_pulses = 0;
        pinMode(pinout.nes_latch, INPUT);
        pinMode(pinout.nes_clock, INPUT);
        attachInterrupt(digitalPinToInterrupt(pinout.nes_latch), latch_irq, RISING);
        attachInterrupt(digitalPinToInterrupt(pinout.nes_clock), clock_irq, FALLING);
        delay(250);
        detachInterrupt(digitalPinToInterrupt(pinout.nes_latch));
        detachInterrupt(digitalPinToInterrupt(pinout.nes_clock));

        if (latch_pulses && (clock_pulses / latch_pulses) > 8) {
            return COMMS_BACKEND_SNES;
        }
        if (latch_pulses && (clock_pulses / latch_pulses) > 1) {
            return COMMS_BACKEND_NES;
        }
    }

    // GameCube / N64 (Joybus) on the known data pin, as on the pico: probe it as
    // an N64 controller first, then GameCube; whichever the host actually polls
    // is the console attached.
    if (pinout.joybus_data > -1) {
        if (N64Console(pinout.joybus_data).DetectActive(JOYBUS_PROBE_MS) == 1) {
            return COMMS_BACKEND_N64;
        }
        if (GamecubeConsole(pinout.joybus_data).DetectActive(JOYBUS_PROBE_MS) == 1) {
            return COMMS_BACKEND_GAMECUBE;
        }
    }

    return COMMS_BACKEND_UNSPECIFIED;
}
