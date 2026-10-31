#include "comms/SnesBackend.hpp"

#include "core/CommunicationBackend.hpp"
#include "core/state.hpp"

SnesBackend::SnesBackend(
    InputState &inputs,
    InputSource **input_sources,
    size_t input_source_count,
    uint8_t data_pin,
    uint8_t clock_pin,
    uint8_t latch_pin
)
    : CommunicationBackend(inputs, input_sources, input_source_count),
      _snes(data_pin, clock_pin, latch_pin) {
    _report = default_snes_report;
    _snes.Init();
}

CommunicationBackendId SnesBackend::BackendId() {
    return COMMS_BACKEND_SNES;
}

void SnesBackend::SendReport() {
    ScanInputs();
    UpdateOutputs();

    // Digital outputs - mapping identical to HAL/pico SnesBackend.
    _report.a = _outputs.a;
    _report.b = _outputs.b;
    _report.x = _outputs.x;
    _report.y = _outputs.y;
    _report.l = _outputs.buttonL || _outputs.triggerLDigital;
    _report.r = _outputs.buttonR || _outputs.triggerRDigital;
    _report.select = _outputs.select;
    _report.start = _outputs.start;
    _report.dpad_left = _outputs.dpadLeft || _outputs.leftStickX < 128;
    _report.dpad_right = _outputs.dpadRight || _outputs.leftStickX > 128;
    _report.dpad_down = _outputs.dpadDown || _outputs.leftStickY < 128;
    _report.dpad_up = _outputs.dpadUp || _outputs.leftStickY > 128;

    _snes.SendReport(_report);
}
