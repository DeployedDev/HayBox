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
    // Turnaround: when we're called the console is still finishing its command
    // (the stop bit). Wait that out - its falling edge, then the line releasing
    // high - plus a short gap, so our reply doesn't collide with it. Without
    // this the reply starts during the stop bit and the host can't decode it
    // (the "traffic seen, handshake never completes" case).
    const uint32_t edge_timeout = _c3 * 4;  // ~12 us
    uint32_t t0 = jb_dwt_now();
    while (JB_READ()) {  // stop-bit falling edge
        if ((jb_dwt_now() - t0) > edge_timeout) break;
    }
    t0 = jb_dwt_now();
    while (!JB_READ()) {  // stop bit released (line high)
        if ((jb_dwt_now() - t0) > edge_timeout) break;
    }
    jb_dwt_wait(_c2);  // ~2 us turnaround before driving the reply

    // Send bits MSB-first on ABSOLUTE per-cell deadlines, so GPIO/loop overhead
    // can't accumulate and drift the 4 us bit cells out of the host's sampling
    // tolerance over a multi-byte reply.
    const uint32_t cell = _c1 + _c3;  // 4 us
    uint32_t next = jb_dwt_now();
    for (uint8_t i = 0; i < len; i++) {
        uint8_t b = data[i];
        for (int bit = 7; bit >= 0; bit--) {
            uint32_t low_dur = ((b >> bit) & 1) ? _c1 : _c3;
            while ((int32_t)(jb_dwt_now() - next) < 0) { }             // cell start -> low
            JB_LOW();
            while ((int32_t)(jb_dwt_now() - (next + low_dur)) < 0) { } // -> release high
            JB_HIGH();
            next += cell;
        }
    }
    while ((int32_t)(jb_dwt_now() - next) < 0) { }  // finish the last cell's high phase
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

void JoybusIO::BeginListen() {
    Init();                       // resolve _pn, timing constants, enable DWT
    pinMode(_pin, INPUT_PULLUP);  // listen only: idle/unconnected pins read HIGH,
                                  // a host pulls LOW. We don't drive until asked.
}

void JoybusIO::EnableDrive() {
    pinMode(_pin, OUTPUT_OPEN_DRAIN);  // now we can pull the line low to respond
    JB_HIGH();                         // released (idle high) to start
}
