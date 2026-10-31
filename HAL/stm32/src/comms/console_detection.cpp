#include "comms/console_detection.hpp"

#include "comms/GamecubeConsole.hpp"
#include "comms/N64Console.hpp"
#include "core/pinout.hpp"

#include <Adafruit_TinyUSB.h>
#include <Arduino.h>
#include <config.pb.h>

/*
 * Frame1 console detection - same order and timing as the pico
 * (HAL/pico/src/comms/console_detection.cpp):
 *
 *   1. count NES/SNES latch/clock pulses while waiting 500 ms;
 *   2. USB host connected      -> XInput (i.e. the default USB backend);
 *   3. else NES/SNES by the clock-to-latch pulse ratio;
 *   4. else GameCube on the known Joybus pin;
 *   5. else N64 on the same pin.
 *
 * Port differences (how, not what):
 *   - The pico core services USB in the background; STM32duino + TinyUSB does
 *     not, so the 500 ms wait pumps the TinyUSB device task.
 *   - The pico reads the USB peripheral's "connected" status bit. Here we use
 *     TinyUSBDevice.mounted() (the host has configured us), which can never be
 *     true without a real host - so it can't misfire when powered from the
 *     console cable. If a PC is slow to finish enumerating, we fall through to
 *     the console probes, find nothing, and still land on the default USB
 *     backend.
 *   - GameCube/N64 are probed actively (DetectActive): we answer the identify
 *     command like a controller and treat the pin as live once the host polls
 *     us, which is what joybus-pio's Detect() does too.
 */

#define JOYBUS_PROBE_MS 60  // active-probe window on the known data pin

static volatile uint32_t latch_pulses = 0;
static volatile uint32_t clock_pulses = 0;
static void latch_irq() { latch_pulses++; }
static void clock_irq() { clock_pulses++; }

CommunicationBackendId detect_console(const Pinout &pinout) {
    const bool nes_pins = pinout.nes_latch > -1 && pinout.nes_clock > -1;

    if (nes_pins) {
        latch_pulses = 0;
        clock_pulses = 0;
        pinMode(pinout.nes_latch, INPUT);
        pinMode(pinout.nes_clock, INPUT);
        attachInterrupt(digitalPinToInterrupt(pinout.nes_latch), latch_irq, RISING);
        attachInterrupt(digitalPinToInterrupt(pinout.nes_clock), clock_irq, FALLING);
    }

    // Wait 500 ms, as the pico does, servicing USB so a connected host can
    // enumerate us in the meantime.
    const uint32_t start = millis();
    while (millis() - start < 500) {
        TinyUSB_Device_Task();
        delay(1);
    }
    const bool usb_connected = TinyUSBDevice.mounted();

    if (nes_pins) {
        detachInterrupt(digitalPinToInterrupt(pinout.nes_latch));
        detachInterrupt(digitalPinToInterrupt(pinout.nes_clock));
    }

    CommunicationBackendId result = COMMS_BACKEND_UNSPECIFIED;

    if (usb_connected) {
        result = COMMS_BACKEND_XINPUT;
    } else if (latch_pulses && (clock_pulses / latch_pulses) > 8) {
        result = COMMS_BACKEND_SNES;
    } else if (latch_pulses && (clock_pulses / latch_pulses) > 1) {
        result = COMMS_BACKEND_NES;
    } else if (GamecubeConsole(pinout.joybus_data).DetectActive(JOYBUS_PROBE_MS) == 1) {
        result = COMMS_BACKEND_GAMECUBE;
    } else if (N64Console(pinout.joybus_data).DetectActive(JOYBUS_PROBE_MS) == 1) {
        result = COMMS_BACKEND_N64;
    }

    return result;
}
