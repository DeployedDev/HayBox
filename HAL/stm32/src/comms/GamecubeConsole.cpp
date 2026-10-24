#include "comms/GamecubeConsole.hpp"

#include <Arduino.h>

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
        if (_io.ReadLine()) continue;  // idle high; keep watching

        noInterrupts();
        uint8_t cmd = 0;
        if (!_io.ReadBytes(&cmd, 1)) {
            interrupts();
            continue;  // framing glitch; resync
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
            default:
                interrupts();
                break;
        }
    }
    return saw_traffic ? 0 : -1;
}

GcPollStatus GamecubeConsole::WaitForPoll() {
    for (;;) {
        // Idle wait with interrupts ENABLED so USB keeps running between polls.
        while (_io.ReadLine()) { /* idle */ }

        // Command burst: mask interrupts for jitter-free bit timing.
        noInterrupts();

        uint8_t cmd = 0;
        if (!_io.ReadBytes(&cmd, 1)) {
            interrupts();
            continue;  // framing glitch; resync
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
            case 0x40: {  // poll / status
                uint8_t args[2];  // mode + rumble
                if (!_io.ReadBytes(args, 2)) {
                    interrupts();
                    break;
                }
                // IRQs stay MASKED: caller must SendReport() immediately.
                return GcPollStatus::OK;
            }
            default:
                interrupts();
                break;
        }
    }
}

void GamecubeConsole::SendReport(const gc_report_t *report) {
    _io.SendBytes(reinterpret_cast<const uint8_t *>(report), sizeof(gc_report_t));
    _io.SendStopBit();
    interrupts();
}
