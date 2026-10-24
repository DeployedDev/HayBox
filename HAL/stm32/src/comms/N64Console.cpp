#include "comms/N64Console.hpp"

#include <Arduino.h>

// N64 controller identity: 0x05 0x00 + status byte. 0x02 = no Controller Pak
// (pak/rumble is not implemented). VERIFY this status byte on hardware - some
// consoles expect 0x01; it affects whether the console issues pak reads.
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
            case 0x02: {  // read Controller Pak (not implemented) -> zeros + CRC
                uint8_t addr[2];
                _io.ReadBytes(addr, 2);
                uint8_t resp[33];
                for (int i = 0; i < 33; i++) resp[i] = 0;
                _io.SendBytes(resp, sizeof(resp));
                _io.SendStopBit();
                interrupts();
                break;
            }
            case 0x03: {  // write Controller Pak (not implemented) -> drain + ack
                uint8_t buf[34];
                _io.ReadBytes(buf, sizeof(buf));
                uint8_t crc = 0;
                _io.SendBytes(&crc, 1);
                _io.SendStopBit();
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
            case 0x02: {  // read Controller Pak (not implemented -> zeros)
                uint8_t addr[2];
                _io.ReadBytes(addr, 2);
                uint8_t resp[33];
                for (int i = 0; i < 33; i++) resp[i] = 0;  // 32 data + CRC byte
                _io.SendBytes(resp, sizeof(resp));
                _io.SendStopBit();
                interrupts();
                break;
            }
            case 0x03: {  // write Controller Pak (not implemented -> drain + ack)
                uint8_t buf[34];  // 2 addr + 32 data
                _io.ReadBytes(buf, sizeof(buf));
                uint8_t crc = 0;
                _io.SendBytes(&crc, 1);
                _io.SendStopBit();
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
