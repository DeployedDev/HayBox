#include "comms/JoybusIO.hpp"

#include <Arduino.h>

/*
 * See JoybusIO.hpp. [DRAFT - not compiled against a specific STM32duino core,
 * not timing-validated.] Most likely tuning points on hardware:
 *   1. digitalWriteFast()/digitalReadFast() with a PinName - if the core lacks
 *      them, fall back to LL_GPIO_* on the resolved port/pin.
 *   2. _c1/_c2/_c3 (bit-cell sub-timings and the RX sample point).
 */

#define JB_LOW()   digitalWriteFast(_pn, LOW)
#define JB_HIGH()  digitalWriteFast(_pn, HIGH)
#define JB_READ()  (digitalReadFast(_pn) == HIGH)

static inline void jb_dwt_init() {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}
static inline uint32_t jb_dwt_now() { return DWT->CYCCNT; }
static inline void jb_dwt_wait(uint32_t cycles) {
    uint32_t start = DWT->CYCCNT;
    while ((DWT->CYCCNT - start) < cycles) { /* spin */ }
}

JoybusIO::JoybusIO(uint8_t data_pin) : _pin(data_pin), _pn(NC), _c1(0), _c2(0), _c3(0) {}

void JoybusIO::Init() {
    _pn = digitalPinToPinName(_pin);
    pinMode(_pin, OUTPUT_OPEN_DRAIN);
    JB_HIGH();  // release line

    jb_dwt_init();
    uint32_t per_us = SystemCoreClock / 1000000UL;  // 72 on the Frame1
    _c1 = per_us;
    _c2 = 2UL * per_us;
    _c3 = 3UL * per_us;
}

bool JoybusIO::ReadLine() { return JB_READ(); }

void JoybusIO::SendBytes(const uint8_t *data, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
        uint8_t b = data[i];
        for (int bit = 7; bit >= 0; bit--) {
            bool one = (b >> bit) & 1;
            JB_LOW();
            jb_dwt_wait(one ? _c1 : _c3);
            JB_HIGH();
            jb_dwt_wait(one ? _c3 : _c1);
        }
    }
}

void JoybusIO::SendStopBit() {
    JB_LOW();
    jb_dwt_wait(_c1);
    JB_HIGH();
    jb_dwt_wait(_c1);
}

bool JoybusIO::ReadBytes(uint8_t *data, uint8_t len) {
    const uint32_t edge_timeout = _c3 * 4;  // ~12 us idle => end-of-command/error
    for (uint8_t i = 0; i < len; i++) {
        uint8_t b = 0;
        for (int bit = 0; bit < 8; bit++) {
            uint32_t t0 = jb_dwt_now();
            while (JB_READ()) {  // wait for falling edge
                if ((jb_dwt_now() - t0) > edge_timeout) return false;
            }
            jb_dwt_wait(_c2);  // sample ~2 us in
            b = (b << 1) | (JB_READ() ? 1 : 0);
            t0 = jb_dwt_now();
            while (!JB_READ()) {  // wait for line to return high
                if ((jb_dwt_now() - t0) > edge_timeout) break;
            }
        }
        data[i] = b;
    }
    return true;
}

bool JoybusIO::SniffFor(uint8_t wanted, uint32_t budget_ms) {
    uint32_t per_ms = SystemCoreClock / 1000UL;
    uint32_t budget = per_ms * budget_ms;
    uint32_t t0 = jb_dwt_now();
    while ((jb_dwt_now() - t0) < budget) {
        if (JB_READ()) continue;  // idle high; keep watching
        noInterrupts();
        uint8_t op = 0;
        bool ok = ReadBytes(&op, 1);
        interrupts();
        if (ok && op == wanted) return true;
        // Let the rest of this command drain so we re-sync on the next one.
        uint32_t g = jb_dwt_now();
        while (!JB_READ() && (jb_dwt_now() - g) < (_c3 * 8)) { /* drain */ }
    }
    return false;
}
