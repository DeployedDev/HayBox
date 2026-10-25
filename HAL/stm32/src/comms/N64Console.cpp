#include "comms/N64Console.hpp"

#include <Arduino.h>

// N64 controller identity: device 0x0005 (sent 0x05 0x00) + status 0x02 - exactly
// joybus-pio's default_n64_status, i.e. what the pico sends.
//
// Controller Pak commands (0x02 read / 0x03 write) are not handled, as on the
// pico (joybus-pio): they fall into the unknown-command path, which waits for the
// command to finish and sends no reply.
static const uint8_t N64_ID[3] = { 0x05, 0x00, 0x02 };

N64Console::N64Console(uint8_t data_pin) : _io(data_pin) {}

void N64Console::Init() { _io.Init(); }

bool N64Console::Detect() { return _io.SniffFor(0x01, 250); }

int N64Console::DetectActive(uint32_t budget_ms) {
    _io.BeginListen();  // pulled-up listen: floating candidate pins read HIGH

    const uint32_t idle_giveup_ms = 8;  // no traffic within this -> idle pin
    uint32_t start = millis();
    bool saw_traffic = false;
    bool driving = false;

    while ((millis() - start) < budget_ms) {
        if (!saw_traffic && (millis() - start) >= idle_giveup_ms) break;

        // Listen with IRQs masked so the command's first edge is caught on time
        // (see WaitForPoll); each pass is bounded to ~0.5 ms by ListenChunk().
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
            // Real low-going traffic: this is a live line. Enable the open-drain
            // driver so we can answer. The first answer may miss its turnaround
            // window (pin-mode switch); the host re-probes and the next lands.
            _io.EnableDrive();
            driving = true;
        }

        switch (cmd) {
            case 0x00:  // info / probe
            case 0xFF:  // reset
                _io.SendBytes(N64_ID, sizeof(N64_ID));
                _io.SendStopBit();
                interrupts();
                break;
            case 0x01:  // poll: host accepted us and is polling -> this is the pin
                interrupts();
                return 1;
            default:  // unknown command (incl. pak 0x02/0x03): let it finish, no reply
                _io.WaitLineIdle();
                interrupts();
                break;
        }
    }
    return saw_traffic ? 0 : -1;
}

N64PollStatus N64Console::WaitForPoll() {
    for (;;) {
        // Listen with IRQs MASKED, so a command's first falling edge is caught with
        // no interrupt latency and every bit is sampled 2 us after its real edge -
        // the closest software equivalent of the pico's always-listening PIO.
        // After ~0.5 ms with no command (ListenChunk), open a brief IRQ window so
        // SysTick/USB keep running, then resume listening.
        noInterrupts();
        uint8_t cmd = 0;
        int r = _io.ReadBytesEx(&cmd, 1, _io.ListenChunk());
        if (r <= 0) {
            if (r < 0) _io.WaitLineIdle();  // framing error mid-command: resync
            interrupts();
            __ISB();
            continue;
        }

        switch (cmd) {
            case 0x00:  // info / probe
            case 0xFF:  // reset
                _io.SendBytes(N64_ID, sizeof(N64_ID));
                _io.SendStopBit();
                interrupts();
                break;
            case 0x01:  // poll / status
                // IRQs stay MASKED: caller must SendReport() immediately.
                return N64PollStatus::OK;
            default:  // unknown command (incl. pak 0x02/0x03): let it finish, no reply
                _io.WaitLineIdle();
                interrupts();
                break;
        }
    }
}

void N64Console::SendReport(const n64_report_t *report) {
    _io.SendBytes(reinterpret_cast<const uint8_t *>(report), sizeof(n64_report_t));
    _io.SendStopBit();
    interrupts();
}
