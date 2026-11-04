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
 *   1. wait 500 ms;
 *   2. USB host connected      -> XInput (i.e. the default USB backend);
 *   3. else GameCube on the known Joybus pin;
 *   4. else N64 on the same pin.
 *
 * Port differences (how, not what):
 *   - The pico also counts NES/SNES latch/clock pulses during the wait. The
 *     Frame1 has no NES/SNES lines (its USB-C port has no spare pins), and in
 *     this tree's Pinout the nes_* fields are uint8_t, so "not wired" (-1)
 *     cannot be expressed: an unset field is pin 0, a real GPIO that would
 *     float and could fake a NES/SNES console. So that step is skipped here.
 *     NES/SNES can still be forced with their plug-in holds, as on the pico.
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

CommunicationBackendId detect_console(const Pinout &pinout) {
    // Wait 500 ms, as the pico does, servicing USB so a connected host can
    // enumerate us in the meantime.
    const uint32_t start = millis();
    while (millis() - start < 500) {
        TinyUSB_Device_Task();
        delay(1);
    }
    const bool usb_connected = TinyUSBDevice.mounted();

    CommunicationBackendId result = COMMS_BACKEND_UNSPECIFIED;

    if (usb_connected) {
        result = COMMS_BACKEND_XINPUT;
    } else if (GamecubeConsole(pinout.joybus_data).DetectActive(JOYBUS_PROBE_MS) == 1) {
        result = COMMS_BACKEND_GAMECUBE;
    } else if (N64Console(pinout.joybus_data).DetectActive(JOYBUS_PROBE_MS) == 1) {
        result = COMMS_BACKEND_N64;
    }

    return result;
}
