#include "comms/GamecubeConsole.hpp"

#include <Arduino.h>

/*
 * STM32 (Frame1) GameCube Joybus device - bit-banged.  [DRAFT - unvalidated]
 *
 * See GamecubeConsole.hpp for the protocol/electrical overview. Everything here
 * is written to the GameCube Joybus spec but has NOT been compiled against a
 * specific STM32duino core version or checked on a logic analyser. The two
 * things most likely to need adjustment on real hardware:
 *   1. Fast GPIO: this uses digitalWriteFast()/digitalReadFast() with a PinName.
 *      If the core version lacks them, drop to LL_GPIO_* on the resolved port.
 *   2. The us->cycle timing constants (bit cell widths, RX sample point).
 */

// ---- fast single-wire IO (open-drain: 0 drives low, 1 releases to pull-up) --
#define GC_LOW()   digitalWriteFast(_pn, LOW)
#define GC_HIGH()  digitalWriteFast(_pn, HIGH)
#define GC_READ()  (digitalReadFast(_pn) == HIGH)

// ---- DWT cycle counter --------------------------------------------------
static inline void dwt_init() {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}
static inline uint32_t dwt_now() { return DWT->CYCCNT; }
static inline void dwt_wait(uint32_t cycles) {
    uint32_t start = DWT->CYCCNT;
    while ((DWT->CYCCNT - start) < cycles) { /* spin */ }
}

// GameCube device identity (standard wired controller with rumble).
static const uint8_t GC_ID[3] = { 0x09, 0x00, 0x03 };

// Neutral origin: 2 button bytes (high1 set) + 4 centred axes + 2 analog + 2
// trailing zero bytes = 10 bytes.
static const uint8_t GC_ORIGIN[10] = {
    0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00
};

GamecubeConsole::GamecubeConsole(uint8_t data_pin)
    : _pin(data_pin), _pn(NC), _c1(0), _c2(0), _c3(0) {}

void GamecubeConsole::Init() {
    _pn = digitalPinToPinName(_pin);
    pinMode(_pin, OUTPUT_OPEN_DRAIN);
    GC_HIGH();  // release the line

    dwt_init();
    uint32_t per_us = SystemCoreClock / 1000000UL;  // 72 on the Frame1
    _c1 = per_us;         // 1 us
    _c2 = 2UL * per_us;   // 2 us (RX sample point)
    _c3 = 3UL * per_us;   // 3 us
}

// ---- TX -----------------------------------------------------------------
// A Joybus bit is a 4 us cell: 0 = 3us low + 1us high, 1 = 1us low + 3us high.
static inline void send_bit(
    bool one,
    PinName pn,
    uint32_t c1,
    uint32_t c3
) {
    digitalWriteFast(pn, LOW);
    dwt_wait(one ? c1 : c3);
    digitalWriteFast(pn, HIGH);
    dwt_wait(one ? c3 : c1);
}

void GamecubeConsole::SendBytes(const uint8_t *data, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
        uint8_t b = data[i];
        for (int bit = 7; bit >= 0; bit--) {
            send_bit((b >> bit) & 1, _pn, _c1, _c3);
        }
    }
}

void GamecubeConsole::SendStopBit() {
    // Stop bit: ~1 us low then release.
    GC_LOW();
    dwt_wait(_c1);
    GC_HIGH();
    dwt_wait(_c1);
}

// ---- RX -----------------------------------------------------------------
// Read `len` bytes MSB-first. Each bit: wait for the falling edge, sample ~2 us
// in, then wait for the line to return high. Returns false on timeout (used as
// end-of-command / framing-error detection). ~ a few bit-cells of slack.
bool GamecubeConsole::ReadBytes(uint8_t *data, uint8_t len) {
    const uint32_t edge_timeout = _c3 * 4;  // ~12 us with no edge => done/error
    for (uint8_t i = 0; i < len; i++) {
        uint8_t b = 0;
        for (int bit = 0; bit < 8; bit++) {
            // Wait for falling edge (line currently high -> goes low).
            uint32_t t0 = dwt_now();
            while (GC_READ()) {
                if ((dwt_now() - t0) > edge_timeout) return false;
            }
            // Sample ~2 us after the falling edge.
            dwt_wait(_c2);
            b = (b << 1) | (GC_READ() ? 1 : 0);
            // Wait for the line to go back high before the next bit.
            t0 = dwt_now();
            while (!GC_READ()) {
                if ((dwt_now() - t0) > edge_timeout) {
                    // Line stuck low past this bit: treat remaining as framed in.
                    break;
                }
            }
        }
        data[i] = b;
    }
    return true;
}

// ---- public protocol ----------------------------------------------------
bool GamecubeConsole::Detect() {
    // A present console polls continuously. Sample the line for a short window;
    // if it ever reads low, a console is driving it. Interrupts stay ENABLED -
    // we only care whether the line was ever pulled low, so ISR jitter is fine,
    // and this runs during setup() where USB must keep being serviced.
    uint32_t per_ms = SystemCoreClock / 1000UL;
    uint32_t budget = per_ms * 200UL;  // ~200 ms
    uint32_t t0 = dwt_now();
    while ((dwt_now() - t0) < budget) {
        if (!GC_READ()) return true;
    }
    return false;
}

GcPollStatus GamecubeConsole::WaitForPoll() {
    for (;;) {
        // Idle wait with interrupts ENABLED so USB keeps running between polls.
        // Spin until the line drops (start of a command), with no hard timeout.
        while (GC_READ()) { /* idle */ }

        // Command burst: mask interrupts so bit timing is jitter-free.
        noInterrupts();

        uint8_t cmd = 0;
        // We are already at/near the first falling edge; read the opcode. Re-use
        // ReadBytes which re-finds each falling edge (the first inner wait will
        // return immediately as the line is already low-ish).
        if (!ReadBytes(&cmd, 1)) {
            interrupts();
            continue;  // framing glitch; resync
        }

        switch (cmd) {
            case 0x00:  // info / probe
            case 0xFF:  // reset
                SendBytes(GC_ID, sizeof(GC_ID));
                SendStopBit();
                interrupts();
                break;  // not a poll; keep waiting
            case 0x41:  // get origin
            case 0x42:  // recalibrate / get origin
                SendBytes(GC_ORIGIN, sizeof(GC_ORIGIN));
                SendStopBit();
                interrupts();
                break;
            case 0x40: {  // poll / status
                uint8_t args[2];  // mode + rumble
                if (!ReadBytes(args, 2)) {
                    interrupts();
                    break;  // bad poll; resync
                }
                // Leave IRQs MASKED and return OK: caller must SendReport() now.
                return GcPollStatus::OK;
            }
            default:
                interrupts();
                break;  // unknown command; ignore and resync
        }
    }
}

void GamecubeConsole::SendReport(const gc_report_t *report) {
    // Called immediately after WaitForPoll()==OK, with IRQs still masked.
    SendBytes(reinterpret_cast<const uint8_t *>(report), sizeof(gc_report_t));
    SendStopBit();
    interrupts();
}
