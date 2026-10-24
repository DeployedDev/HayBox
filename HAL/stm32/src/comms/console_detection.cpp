#include "comms/console_detection.hpp"

#include "comms/JoybusIO.hpp"
#include "core/pinout.hpp"

#include <Arduino.h>
#include <config.pb.h>

/*
 * Frame1 console auto-detection.
 *
 * NES/SNES: counted on the nes_* lines by latch/clock pulse ratio (as on the
 * pico). GameCube/N64 (Joybus): the data line is carried on one of the board's
 * USB-C pins to an STM32 GPIO whose identity isn't documented, so instead of
 * assuming a pin we SCAN the free GPIOs for one that a console/adapter is
 * actively polling, and bind the backend to it. The poll opcode distinguishes
 * GameCube (0x40) from N64 (0x01). The discovered pin is published via
 * g_discovered_joybus_pin for backend construction. Returns UNSPECIFIED (->
 * default USB backend) when nothing is polling.
 *
 * NOTE: in the default path (nothing held, not on a console) the scan adds up
 * to (candidates * JOYBUS_SCAN_MS) ms to boot. Once the data pin is known it
 * can be hard-set in pinout.joybus_data to skip the scan entirely.
 */

// Published to backend_init so the GameCube/N64 backend is built on the pin the
// scan found. 0xFF = not discovered (fall back to pinout.joybus_data).
uint8_t g_discovered_joybus_pin = 0xFF;

#define JOYBUS_SCAN_MS 30

// Free GPIOs - everything NOT used by the button matrix, USB (PA11/PA12), SWD
// (PA13/PA14), or the HSE crystal. The Joybus data line is one of these.
static const uint8_t kJoybusCandidates[] = {
    PA0,  PA1,  PA2,  PA3,  PA4,  PA5,  PA6,  PA7,  PA8,  PA9,  PA10, PA15,
    PB3,  PB4,  PB5,  PB6,  PB7,  PB8,  PB9,  PB11, PB12, PB13, PB14, PB15,
    PC0,  PC1,  PC2,  PC3,  PC6,  PC7,  PC8,  PC9,  PC10, PC11, PC12, PC13, PC14, PC15,
    PD0,  PD1,  PD3,  PD4,  PD5,  PD6,  PD7,  PD8,  PD9,  PD10, PD11, PD12, PD13, PD14, PD15,
    PE0,  PE1,  PE6,
};
static const size_t kJoybusCandidateCount =
    sizeof(kJoybusCandidates) / sizeof(kJoybusCandidates[0]);

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

    // GameCube / N64: scan the free GPIOs for a pin a host is actively polling.
    for (size_t i = 0; i < kJoybusCandidateCount; i++) {
        JoybusIO io(kJoybusCandidates[i]);
        int op = io.DetectPoll(JOYBUS_SCAN_MS);
        if (op == 0x40) {
            g_discovered_joybus_pin = kJoybusCandidates[i];
            return COMMS_BACKEND_GAMECUBE;
        }
        if (op == 0x01) {
            g_discovered_joybus_pin = kJoybusCandidates[i];
            return COMMS_BACKEND_N64;
        }
    }

    return COMMS_BACKEND_UNSPECIFIED;
}
