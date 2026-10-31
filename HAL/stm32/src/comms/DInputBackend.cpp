#include "comms/DInputBackend.hpp"

#include "core/CommunicationBackend.hpp"
#include "core/state.hpp"

#include <TUGamepad.hpp>

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
    ScanInputs(InputScanSpeed::SLOW);
    ScanInputs(InputScanSpeed::MEDIUM);

    // Wait until the host is ready for the next report. On the RP2040 core this
    // spins on tight_loop_contents(); on STM32duino we yield(), which also pumps
    // the TinyUSB device task so the stack keeps servicing control transfers.
    while (!_gamepad.ready()) {
        yield();
    }

    ScanInputs(InputScanSpeed::FAST);

    UpdateOutputs();

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

    _gamepad.sendState();
}
