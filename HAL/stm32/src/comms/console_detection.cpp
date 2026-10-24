#include "comms/console_detection.hpp"

#include "comms/GamecubeConsole.hpp"
#include "comms/N64Console.hpp"
#include "core/pinout.hpp"

#include <Arduino.h>
#include <config.pb.h>

/*
 * Frame1 console auto-detection.
 *
 * NES/SNES: counted on the nes_* lines by latch/clock pulse ratio (as on the
 * pico). GameCube/N64 (Joybus): the data line is carried on one of the board's
 * USB-C pins to an STM32 GPIO whose identity isn't documented, so we probe the
 * free GPIOs. The probe is ACTIVE: on each candidate pin we briefly behave as a
 * controller and answer the identify command, because real consoles (and the
 * Raphnet adapter) only start polling AFTER a valid identify reply - a passive
 * listen would deadlock, since nothing polls until we answer. A pin is the data
 * line once the host responds by actually polling us: 0x01 -> N64, 0x40 ->
 * GameCube. The discovered pin is published via g_discovered_joybus_pin for
 * backend construction. Returns UNSPECIFIED (-> default USB backend) when no
 * pin is driven.
 *
 * NOTE: idle candidate pins bail in a few ms, so a no-console boot adds only
 * ~(candidates * idle_giveup) ms. Once the data pin is known it can be hard-set
 * in pinout.joybus_data to skip the probe entirely (pico parity).
 */

// Published to backend_init so the GameCube/N64 backend is built on the pin the
// scan found. 0xFF = not discovered (fall back to pinout.joybus_data).
uint8_t g_discovered_joybus_pin = 0xFF;

#define JOYBUS_PROBE_MS 60  // max active-probe window per candidate pin

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

    // GameCube / N64: actively probe each free GPIO. N64 first; if a pin shows
    // Joybus traffic that isn't N64, try GameCube on that same pin.
    for (size_t i = 0; i < kJoybusCandidateCount; i++) {
        uint8_t pin = kJoybusCandidates[i];

        int r = N64Console(pin).DetectActive(JOYBUS_PROBE_MS);
        if (r == 1) {
            g_discovered_joybus_pin = pin;
            return COMMS_BACKEND_N64;
        }
        if (r == 0) {  // live Joybus pin, but not N64 -> is it GameCube?
            if (GamecubeConsole(pin).DetectActive(JOYBUS_PROBE_MS) == 1) {
                g_discovered_joybus_pin = pin;
                return COMMS_BACKEND_GAMECUBE;
            }
        }
        // r == -1: idle pin, next (fast).
    }

    return COMMS_BACKEND_UNSPECIFIED;
}
