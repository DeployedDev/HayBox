#include "comms/ShiftRegisterIO.hpp"

#include <Arduino.h>

/*
 * See ShiftRegisterIO.hpp. [DRAFT - not compiled against a specific STM32duino
 * core, not timing-validated.] The latch/clock edge handling and the exact
 * bit-vs-clock alignment are the things to confirm on a logic analyser.
 */

#define SR_DATA_LOW()   digitalWriteFast(_data, LOW)
#define SR_DATA_HIGH()  digitalWriteFast(_data, HIGH)
#define SR_CLOCK_HIGH() (digitalReadFast(_clock) == HIGH)
#define SR_LATCH_HIGH() (digitalReadFast(_latch) == HIGH)

static inline uint32_t cyc_now() { return DWT->CYCCNT; }

ShiftRegisterIO::ShiftRegisterIO(uint8_t data_pin, uint8_t clock_pin, uint8_t latch_pin)
    : _data_pin(data_pin), _clock_pin(clock_pin), _latch_pin(latch_pin),
      _data(NC), _clock(NC), _latch(NC) {}

void ShiftRegisterIO::Init() {
    _data = digitalPinToPinName(_data_pin);
    _clock = digitalPinToPinName(_clock_pin);
    _latch = digitalPinToPinName(_latch_pin);

    pinMode(_data_pin, OUTPUT_OPEN_DRAIN);
    SR_DATA_HIGH();  // released (not pressed)
    pinMode(_clock_pin, INPUT);
    pinMode(_latch_pin, INPUT);

    // DWT cycle counter (harmless if JoybusIO already enabled it).
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

bool ShiftRegisterIO::RespondToPoll(const bool *pressed, uint8_t nbits) {
    uint32_t per_ms = SystemCoreClock / 1000UL;
    uint32_t latch_timeout = per_ms * 50UL;  // console latches ~60 Hz
    uint32_t edge_timeout = per_ms;          // ~1 ms per clock edge

    // Wait for a latch pulse (rising), interrupts ENABLED.
    uint32_t t0 = cyc_now();
    while (!SR_LATCH_HIGH()) {
        if ((cyc_now() - t0) > latch_timeout) return false;
    }

    // Latched: present bit 0, then clock out the rest with interrupts masked.
    noInterrupts();
    SR_DATA_HIGH();
    if (pressed[0]) SR_DATA_LOW();  // active-low

    // Wait for latch to fall.
    t0 = cyc_now();
    while (SR_LATCH_HIGH()) {
        if ((cyc_now() - t0) > edge_timeout) break;
    }

    for (uint8_t i = 1; i < nbits; i++) {
        // Wait for clock falling edge (clock idles high).
        t0 = cyc_now();
        while (SR_CLOCK_HIGH()) {
            if ((cyc_now() - t0) > edge_timeout) {
                interrupts();
                return true;  // console stopped clocking; frame done/aborted
            }
        }
        // Present bit i while clock is low; console samples on the rising edge.
        SR_DATA_HIGH();
        if (pressed[i]) SR_DATA_LOW();
        // Wait for clock to return high.
        t0 = cyc_now();
        while (!SR_CLOCK_HIGH()) {
            if ((cyc_now() - t0) > edge_timeout) break;
        }
    }

    SR_DATA_HIGH();  // release after the frame
    interrupts();
    return true;
}
