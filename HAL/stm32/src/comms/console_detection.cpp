#include "comms/console_detection.hpp"

#include "comms/GamecubeConsole.hpp"
#include "comms/N64Console.hpp"
#include "core/pinout.hpp"

#include <Arduino.h>
#include <config.pb.h>

/*
 * Frame1 console auto-detection (mirrors HAL/pico, minus the RP2040
 * USB-connected fast-path). NES vs SNES is told apart by how many clock pulses
 * the console sends per latch (~8 = NES, ~16 = SNES); GameCube vs N64 by which
 * poll opcode appears on the Joybus line. Returns UNSPECIFIED when nothing is
 * driving the lines, so initialize_backends() falls through to the default USB
 * backend (XInput).
 */

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

    // GameCube / N64 on the Joybus line.
    GamecubeConsole gamecube(pinout.joybus_data);
    gamecube.Init();
    if (gamecube.Detect()) {
        return COMMS_BACKEND_GAMECUBE;
    }

    N64Console n64(pinout.joybus_data);
    n64.Init();
    if (n64.Detect()) {
        return COMMS_BACKEND_N64;
    }

    return COMMS_BACKEND_UNSPECIFIED;
}
