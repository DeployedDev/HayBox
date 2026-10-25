#include "comms/N64Console.hpp"

#include <Arduino.h>

// N64 controller identity: device 0x0005 (sent 0x05 0x00) + status 0x02. This is
// byte-for-byte joybus-pio's default_n64_status, i.e. exactly what the pico sends.
// Controller Pak commands (0x02 read / 0x03 write) are NOT answered: they are
// drained to stay in sync and otherwise ignored, as on the pico. A fake pak
// reply from a controller that reports "no pak" confuses games (OoT shows
// "No Controller"), so no stub responses here.
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
        if (_io.ReadLine()) continue;  // idle high; keep watching

        noInterrupts();
        uint8_t cmd = 0;
        if (!_io.ReadBytes(&cmd, 1)) {
            interrupts();
            continue;  // framing glitch; resync
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
            case 0x02: {  // read Controller Pak: drain the address, no reply (as on the pico)
                uint8_t addr[2];
                _io.ReadBytes(addr, 2);
                interrupts();
                break;
            }
            case 0x03: {  // write Controller Pak: drain addr + data, no reply (as on the pico)
                uint8_t buf[34];
                _io.ReadBytes(buf, sizeof(buf));
                interrupts();
                break;
            }
            default:
                interrupts();
                break;
        }
    }
    return saw_traffic ? 0 : -1;
}

N64PollStatus N64Console::WaitForPoll() {
    for (;;) {
        while (_io.ReadLine()) { /* idle, IRQs on */ }

        noInterrupts();

        uint8_t cmd = 0;
        if (!_io.ReadBytes(&cmd, 1)) {
            interrupts();
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
            case 0x02: {  // read Controller Pak: drain the address, no reply (as on the pico)
                uint8_t addr[2];
                _io.ReadBytes(addr, 2);
                interrupts();
                break;
            }
            case 0x03: {  // write Controller Pak: drain addr + data, no reply (as on the pico)
                uint8_t buf[34];  // 2 addr + 32 data
                _io.ReadBytes(buf, sizeof(buf));
                interrupts();
                break;
            }
            default:
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
