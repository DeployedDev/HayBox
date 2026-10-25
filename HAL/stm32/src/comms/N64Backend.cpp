#include "comms/N64Backend.hpp"

#include "core/CommunicationBackend.hpp"
#include "core/state.hpp"

N64Backend::N64Backend(
    InputState &inputs,
    InputSource **input_sources,
    size_t input_source_count,
    uint8_t data_pin
)
    : CommunicationBackend(inputs, input_sources, input_source_count),
      _n64(data_pin) {
    _report = default_n64_report;
    _n64.Init();
}

CommunicationBackendId N64Backend::BackendId() {
    return COMMS_BACKEND_N64;
}

void N64Backend::SendReport() {
    // Same order as the pico: update slower inputs before waiting for the poll,
    // then read the fast inputs (the buttons) and run the game mode in response
    // to the poll, so the report reflects the inputs at poll time rather than
    // as they were one poll earlier. Mapping matches HAL/pico.
    ScanInputs(InputScanSpeed::SLOW);
    ScanInputs(InputScanSpeed::MEDIUM);

    // Leaves IRQs masked until SendReport() below.
    if (_n64.WaitForPoll() != N64PollStatus::OK) {
        return;
    }

    ScanInputs(InputScanSpeed::FAST);
    UpdateOutputs();

    // Digital outputs.
    _report.a = _outputs.a;
    _report.b = _outputs.b;
    _report.z = _outputs.buttonR;
    _report.l = _outputs.triggerLDigital;
    _report.r = _outputs.triggerRDigital;
    _report.start = _outputs.start;
    _report.dpad_left = _outputs.dpadLeft;
    _report.dpad_right = _outputs.dpadRight;
    _report.dpad_down = _outputs.dpadDown;
    _report.dpad_up = _outputs.dpadUp;
    // Right stick -> C-pad (same mapping as the pico backend).
    _report.c_left = _outputs.rightStickX < 128;
    _report.c_right = _outputs.rightStickX > 128;
    _report.c_down = _outputs.rightStickY < 128;
    _report.c_up = _outputs.rightStickY > 128;

    // Analog stick: 0..255 (centre 128) -> signed centre 0.
    _report.stick_x = _outputs.leftStickX - 128;
    _report.stick_y = _outputs.leftStickY - 128;

    _n64.SendReport(&_report);
}
