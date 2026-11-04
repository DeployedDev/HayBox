#include "comms/GamecubeConsole.hpp"

#include "comms/MeleeTicker.hpp"

#include <Arduino.h>

// Melee ticks are kept this far (plus the longest tick seen) ahead of the next
// expected poll, so a tick is never running when a poll starts.
#define POLL_GUARD_MARGIN_US 60
// Poll intervals outside this range are ignored when learning the poll rate
// (a pause, or two commands that were not a regular poll pair).
#define POLL_INTERVAL_MIN_US 300
#define POLL_INTERVAL_MAX_US 50000
// If the expected poll is this overdue, assume the console stopped polling
// (e.g. paused) and let ticks run again.
#define POLL_OVERDUE_GIVEUP_US 2000

// GameCube device identity (standard wired controller with rumble).
static const uint8_t GC_ID[3] = { 0x09, 0x00, 0x03 };

// Neutral origin: 2 button bytes (high1 set) + 4 centred axes + 2 analog + 2
// trailing zero bytes.
static const uint8_t GC_ORIGIN[10] = {
    0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00
};

GamecubeConsole::GamecubeConsole(uint8_t data_pin) : _io(data_pin) {}

void GamecubeConsole::Init() { _io.Init(); }

bool GamecubeConsole::Detect() { return _io.SniffFor(0x40, 250); }

int GamecubeConsole::DetectActive(uint32_t budget_ms) {
    _io.BeginListen();  // pulled-up listen: floating candidate pins read HIGH

    const uint32_t idle_giveup_ms = 8;  // no traffic within this -> idle pin
    uint32_t start = millis();
    bool saw_traffic = false;
    bool driving = false;

    while ((millis() - start) < budget_ms) {
        if (!saw_traffic && (millis() - start) >= idle_giveup_ms) break;

        // Listen with IRQs masked so the command's first edge is caught on time
        // (see WaitForPollStart); each pass is bounded to ~0.5 ms by ListenChunk().
        noInterrupts();
        uint8_t cmd = 0;
        int r = _io.ReadBytesEx(&cmd, 1, _io.ListenChunk());
        if (r <= 0) {
            if (r < 0) _io.WaitLineIdle();  // framing error mid-command: resync
            interrupts();
            __ISB();  // brief IRQ window (SysTick / millis)
            continue;
        }
        saw_traffic = true;
        if (!driving) {
            _io.EnableDrive();
            driving = true;
        }

        switch (cmd) {
            case 0x00:  // info / probe
            case 0xFF:  // reset
                _io.SendBytes(GC_ID, sizeof(GC_ID));
                _io.SendStopBit();
                interrupts();
                break;
            case 0x41:  // get origin
            case 0x42:  // recalibrate / get origin
                _io.SendBytes(GC_ORIGIN, sizeof(GC_ORIGIN));
                _io.SendStopBit();
                interrupts();
                break;
            case 0x40: {  // poll: host accepted us and is polling -> this is the pin
                uint8_t args[2];  // mode + rumble
                _io.ReadBytes(args, 2);
                interrupts();
                return 1;
            }
            default:  // unknown command: let it finish so its data isn't read as a new one
                _io.WaitLineIdle();
                interrupts();
                break;
        }
    }
    return saw_traffic ? 0 : -1;
}

bool GamecubeConsole::TickIsSafe(uint32_t now_us) const {
    if (_poll_count < 2) {
        return false;  // poll rate not known yet: only catch up after polls
    }
    uint32_t period = 0;
    const uint8_t n = _poll_count - 1 < 4 ? _poll_count - 1 : 4;
    for (uint8_t i = 0; i < n; i++) {
        const uint32_t v = _poll_intervals_us[i];
        if (v != 0 && (period == 0 || v < period)) {
            period = v;  // shortest recent interval: the safest assumption
        }
    }
    if (period == 0) {
        return false;
    }
    const int32_t until_poll = (int32_t)(_last_poll_us + period - now_us);
    if (until_poll < -(int32_t)POLL_OVERDUE_GIVEUP_US) {
        return true;  // console seems to have stopped polling
    }
    return until_poll > (int32_t)(melee_ticker_max_tick_us() + POLL_GUARD_MARGIN_US);
}

void GamecubeConsole::NotePoll(uint32_t now_us) {
    if (_poll_count > 0) {
        const uint32_t interval = now_us - _last_poll_us;
        if (interval >= POLL_INTERVAL_MIN_US && interval <= POLL_INTERVAL_MAX_US) {
            for (uint8_t i = 3; i > 0; i--) {
                _poll_intervals_us[i] = _poll_intervals_us[i - 1];
            }
            _poll_intervals_us[0] = interval;
        }
    }
    _last_poll_us = now_us;
    if (_poll_count < 255) {
        _poll_count++;
    }
}

GcPollStatus GamecubeConsole::WaitForPollStart(bool service_melee_ticks) {
    const uint32_t cycles_per_us = SystemCoreClock / 1000000UL;
    for (;;) {
        uint32_t timeout = _io.ListenChunk();

        if (service_melee_ticks) {
            // IRQs are enabled here. Run a due Melee tick if no poll is expected
            // before it would finish; otherwise it stays due and the backend
            // runs it right after replying. Then listen only until the next
            // tick is due, so ticks stay on their 500 us grid.
            uint32_t now = micros();
            if (melee_ticker_us_until_due(now) <= 0 && TickIsSafe(now)) {
                melee_ticker_run(now);
                now = micros();
            }
            const int32_t until_tick = melee_ticker_us_until_due(now);
            if (until_tick > 0 && (uint32_t)until_tick * cycles_per_us < timeout) {
                timeout = (uint32_t)until_tick * cycles_per_us;
            }
        }

        // Listen with IRQs MASKED, so a command's first falling edge is caught with
        // no interrupt latency and every bit is sampled 2 us after its real edge -
        // the closest software equivalent of the pico's always-listening PIO.
        // After ~0.5 ms with no command (ListenChunk), open a brief IRQ window so
        // SysTick/USB keep running, then resume listening.
        noInterrupts();
        uint8_t cmd = 0;
        int r = _io.ReadBytesEx(&cmd, 1, timeout);
        if (r <= 0) {
            if (r < 0) _io.WaitLineIdle();  // framing error mid-command: resync
            interrupts();
            __ISB();
            continue;
        }

        switch (cmd) {
            case 0x00:  // info / probe
            case 0xFF:  // reset
                _io.SendBytes(GC_ID, sizeof(GC_ID));
                _io.SendStopBit();
                interrupts();
                break;
            case 0x41:  // get origin
            case 0x42:  // recalibrate / get origin
                _io.SendBytes(GC_ORIGIN, sizeof(GC_ORIGIN));
                _io.SendStopBit();
                interrupts();
                break;
            case 0x40:  // poll: first byte received
                NotePoll(micros());
                // Return now, with IRQs MASKED, so the caller can read inputs and
                // build the report while the poll's two argument bytes (mode +
                // rumble) are still arriving - as the pico does. They aren't read;
                // WaitForPollEnd() waits for the command to finish.
                return GcPollStatus::OK;
            default:  // unknown command: let it finish so its data isn't read as a new one
                _io.WaitLineIdle();
                interrupts();
                break;
        }
    }
}

GcPollStatus GamecubeConsole::WaitForPollEnd() {
    _io.WaitLineIdle();  // rest of the poll (2 bytes) + stop bit
    return GcPollStatus::OK;
}

void GamecubeConsole::SendReport(const gc_report_t *report) {
    // WaitForPollEnd() already waited for the line to go idle.
    _io.SendBytes(reinterpret_cast<const uint8_t *>(report), sizeof(gc_report_t), false);
    _io.SendStopBit();
    interrupts();
}
