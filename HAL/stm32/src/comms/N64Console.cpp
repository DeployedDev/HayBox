#include "comms/N64Console.hpp"

#include <Arduino.h>

// N64 controller identity: device 0x0005 (sent 0x05 0x00) + status 0x02 - exactly
// joybus-pio's default_n64_status, i.e. what the pico sends.
//
// Controller Pak commands (0x02 read / 0x03 write): the pico does NOT answer them
// (they fall through unhandled). We currently answer them minimally - a valid
// "empty pak" read (32 zero bytes + CRC 0x00) and a write ack. This was restored
// after a test in which dropping them appeared to break the Raphnet adapter, but
// that test was confounded (the adapter was stuck in raw mode at the time), so
// whether the adapter needs these replies is UNVERIFIED. Re-test the
// pico-faithful behaviour (no reply) once the transport is solid.
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
            case 0x02: {  // read Controller Pak -> valid "empty pak" reply (see note above)
                uint8_t addr[2];
                _io.ReadBytes(addr, 2);
                uint8_t resp[33];
                for (int i = 0; i < 33; i++) resp[i] = 0;  // 32 data + CRC (CRC of zeros = 0)
                _io.SendBytes(resp, sizeof(resp));
                _io.SendStopBit();
                interrupts();
                break;
            }
            case 0x03: {  // write Controller Pak -> drain + ack (see note above)
                uint8_t buf[34];
                _io.ReadBytes(buf, sizeof(buf));
                uint8_t crc = 0;
                _io.SendBytes(&crc, 1);
                _io.SendStopBit();
                interrupts();
                break;
            }
            default:  // unknown command: let it finish so its data isn't read as a new one
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
            case 0x02: {  // read Controller Pak -> valid "empty pak" reply (see note above)
                uint8_t addr[2];
                _io.ReadBytes(addr, 2);
                uint8_t resp[33];
                for (int i = 0; i < 33; i++) resp[i] = 0;  // 32 data + CRC (CRC of zeros = 0)
                _io.SendBytes(resp, sizeof(resp));
                _io.SendStopBit();
                interrupts();
                break;
            }
            case 0x03: {  // write Controller Pak -> drain + ack (see note above)
                uint8_t buf[34];  // 2 addr + 32 data
                _io.ReadBytes(buf, sizeof(buf));
                uint8_t crc = 0;
                _io.SendBytes(&crc, 1);
                _io.SendStopBit();
                interrupts();
                break;
            }
            default:  // unknown command: let it finish so its data isn't read as a new one
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
