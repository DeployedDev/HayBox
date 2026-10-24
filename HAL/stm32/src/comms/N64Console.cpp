#include "comms/N64Console.hpp"

#include <Arduino.h>

// N64 controller identity: 0x05 0x00 + status byte. 0x02 = no Controller Pak
// (pak/rumble is not implemented). VERIFY this status byte on hardware - some
// consoles expect 0x01; it affects whether the console issues pak reads.
static const uint8_t N64_ID[3] = { 0x05, 0x00, 0x02 };

N64Console::N64Console(uint8_t data_pin) : _io(data_pin) {}

void N64Console::Init() { _io.Init(); }

bool N64Console::Detect() { return _io.SniffFor(0x01, 250); }

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
