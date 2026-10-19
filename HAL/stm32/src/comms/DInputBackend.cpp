#include "comms/DInputBackend.hpp"

#include "core/CommunicationBackend.hpp"
#include "core/state.hpp"

#include <TUGamepad.hpp>

/*
 * Mirrors HAL/pico/src/comms/DInputBackend.cpp (ruleset-compliant HayBox). Platform
 * differences only: STM32duino has no background USB task, so the
 * wait-for-ready loop yield()s (runs tud_task()) instead of spinning; and in
 * Melee mode the wait is non-blocking. On the RP2040 the Melee input scanning
 * and output limiter run on the second core at exactly 2 kHz; the single-core
 * F303 runs them from the paced loop in config/frame1/config.cpp, so a report
 * that cannot be sent yet is simply skipped rather than stalling that cadence.
 */

DInputBackend::DInputBackend(
    InputState &inputs,
    InputSource **input_sources,
    size_t input_source_count
)
    : CommunicationBackend(inputs, input_sources, input_source_count) {
    _gamepad.begin();
}

DInputBackend::~DInputBackend() {
    _gamepad.resetInputs();
}

CommunicationBackendId DInputBackend::BackendId() {
    return COMMS_BACKEND_DINPUT;
}

void DInputBackend::SendReport() {
    if (!_gamemode->isMelee()) {
        ScanInputs(InputScanSpeed::SLOW);
        ScanInputs(InputScanSpeed::MEDIUM);
    }

    if (_gamemode->isMelee()) {
        if (!_gamepad.ready()) {
            return;
        }
    } else {
        while (!_gamepad.ready()) {
            yield();
        }
    }

    if (!_gamemode->isMelee()) {
        ScanInputs(InputScanSpeed::FAST);

        UpdateOutputs();
    }

    if (!_gamemode->isMelee()) {
        // Digital outputs
        _gamepad.setButton(0, _outputs.b);
        _gamepad.setButton(1, _outputs.a);
        _gamepad.setButton(2, _outputs.y);
        _gamepad.setButton(3, _outputs.x);
        _gamepad.setButton(4, _outputs.buttonR);
        _gamepad.setButton(5, _outputs.triggerRDigital);
        _gamepad.setButton(6, _outputs.buttonL);
        _gamepad.setButton(7, _outputs.triggerLDigital);
        _gamepad.setButton(8, _outputs.select);
        _gamepad.setButton(9, _outputs.start);
        _gamepad.setButton(10, _outputs.rightStickClick);
        _gamepad.setButton(11, _outputs.leftStickClick);
        _gamepad.setButton(12, _outputs.home);

        // Analog outputs
        _gamepad.leftXAxis(_outputs.leftStickX);
        _gamepad.leftYAxis(255 - _outputs.leftStickY);
        _gamepad.rightXAxis(_outputs.rightStickX);
        _gamepad.rightYAxis(255 - _outputs.rightStickY);
        _gamepad.triggerLAnalog(_outputs.triggerLAnalog + 1);
        _gamepad.triggerRAnalog(_outputs.triggerRAnalog + 1);

        // D-pad Hat Switch
        _gamepad.hatSwitch(_outputs.dpadLeft, _outputs.dpadRight, _outputs.dpadDown, _outputs.dpadUp);
    } else {
        // Digital outputs
        _gamepad.setButton(0, _finalOutputs.b);
        _gamepad.setButton(1, _finalOutputs.a);
        _gamepad.setButton(2, _finalOutputs.y);
        _gamepad.setButton(3, _finalOutputs.x);
        _gamepad.setButton(4, _finalOutputs.buttonR);
        _gamepad.setButton(5, _finalOutputs.triggerRDigital);
        _gamepad.setButton(6, _finalOutputs.buttonL);
        _gamepad.setButton(7, _finalOutputs.triggerLDigital);
        _gamepad.setButton(8, _finalOutputs.select);
        _gamepad.setButton(9, _finalOutputs.start);
        _gamepad.setButton(10, _finalOutputs.rightStickClick);
        _gamepad.setButton(11, _finalOutputs.leftStickClick);
        _gamepad.setButton(12, _finalOutputs.home);

        // Analog outputs
        _gamepad.leftXAxis(_finalOutputs.leftStickX);
        _gamepad.leftYAxis(255 - _finalOutputs.leftStickY);
        _gamepad.rightXAxis(_finalOutputs.rightStickX);
        _gamepad.rightYAxis(255 - _finalOutputs.rightStickY);
        _gamepad.triggerLAnalog(_finalOutputs.triggerLAnalog + 1);
        _gamepad.triggerRAnalog(_finalOutputs.triggerRAnalog + 1);

        // D-pad Hat Switch
        _gamepad.hatSwitch(_finalOutputs.dpadLeft, _finalOutputs.dpadRight, _finalOutputs.dpadDown, _finalOutputs.dpadUp);
    }

    _gamepad.sendState();
}
