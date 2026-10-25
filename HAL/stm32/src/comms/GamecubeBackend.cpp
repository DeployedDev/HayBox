#include "comms/GamecubeBackend.hpp"

#include "core/CommunicationBackend.hpp"
#include "core/state.hpp"

GamecubeBackend::GamecubeBackend(
    InputState &inputs,
    InputSource **input_sources,
    size_t input_source_count,
    uint8_t data_pin
)
    : CommunicationBackend(inputs, input_sources, input_source_count),
      _gamecube(data_pin) {
    _report = default_gc_report;
    _gamecube.Init();
}

CommunicationBackendId GamecubeBackend::BackendId() {
    return COMMS_BACKEND_GAMECUBE;
}

void GamecubeBackend::SendReport() {
    // Same order as the pico: update slower inputs before waiting for the poll,
    // then read the fast inputs (the buttons) and run the game mode while the
    // rest of the poll is still arriving, so the report reflects the inputs at
    // poll time. (The pico also waits 40 us first to sample a little later; we
    // skip that so the reply isn't pushed past the end of the poll.)
    ScanInputs(InputScanSpeed::SLOW);
    ScanInputs(InputScanSpeed::MEDIUM);

    // Leaves IRQs masked until SendReport() below.
    if (_gamecube.WaitForPollStart() != GcPollStatus::OK) {
        return;
    }

    ScanInputs(InputScanSpeed::FAST);
    UpdateOutputs();

    // Digital outputs - mapping identical to HAL/pico GamecubeBackend.
    _report.a = _outputs.a;
    _report.b = _outputs.b;
    _report.x = _outputs.x;
    _report.y = _outputs.y;
    _report.z = _outputs.buttonR;
    _report.l = _outputs.triggerLDigital;
    _report.r = _outputs.triggerRDigital;
    _report.start = _outputs.start;
    _report.dpad_left = _outputs.dpadLeft | _outputs.select;
    _report.dpad_right = _outputs.dpadRight | _outputs.home;
    _report.dpad_down = _outputs.dpadDown;
    _report.dpad_up = _outputs.dpadUp;

    // Analog outputs.
    _report.stick_x = _outputs.leftStickX;
    _report.stick_y = _outputs.leftStickY;
    _report.cstick_x = _outputs.rightStickX;
    _report.cstick_y = _outputs.rightStickY;
    _report.l_analog = _outputs.triggerLAnalog;
    _report.r_analog = _outputs.triggerRAnalog;

    // Send outputs to the console once the poll has finished.
    if (_gamecube.WaitForPollEnd() == GcPollStatus::OK) {
        _gamecube.SendReport(&_report);
    }
}
