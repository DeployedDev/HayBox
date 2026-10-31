#include "comms/NesBackend.hpp"

#include "core/CommunicationBackend.hpp"
#include "core/state.hpp"

NesBackend::NesBackend(
    InputState &inputs,
    InputSource **input_sources,
    size_t input_source_count,
    uint8_t data_pin,
    uint8_t clock_pin,
    uint8_t latch_pin
)
    : CommunicationBackend(inputs, input_sources, input_source_count),
      _nes(data_pin, clock_pin, latch_pin) {
    _report = default_nes_report;
    _nes.Init();
}

CommunicationBackendId NesBackend::BackendId() {
    return COMMS_BACKEND_NES;
}

void NesBackend::SendReport() {
    ScanInputs();
    UpdateOutputs();

    // Digital outputs - mapping identical to HAL/pico NesBackend.
    _report.a = _outputs.a;
    _report.b = _outputs.b;
    _report.select = _outputs.select;
    _report.start = _outputs.start;
    _report.dpad_left = _outputs.dpadLeft || _outputs.leftStickX < 128;
    _report.dpad_right = _outputs.dpadRight || _outputs.leftStickX > 128;
    _report.dpad_down = _outputs.dpadDown || _outputs.leftStickY < 128;
    _report.dpad_up = _outputs.dpadUp || _outputs.leftStickY > 128;

    _nes.SendReport(_report);
}
