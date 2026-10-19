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
    // On the pico the PIO reads the poll command in the background while the CPU
    // scans inputs; the STM32 bit-bang can't, so we scan + build the report
    // FIRST, then fire it the instant the poll arrives (GamecubeConsole's call
    // contract: SendReport must immediately follow WaitForPoll()==OK). Costs up
    // to one poll of input latency versus the pico, but keeps the Joybus
    // command->response turnaround inside the console's timing window.
    ScanInputs();
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

    // Wait for the console poll, then respond immediately (IRQs are masked
    // between these two calls by the console; do nothing else in between).
    if (_gamecube.WaitForPoll() == GcPollStatus::OK) {
        _gamecube.SendReport(&_report);
    }
}
