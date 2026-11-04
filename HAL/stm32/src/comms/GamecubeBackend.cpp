#include "comms/GamecubeBackend.hpp"

#include "comms/MeleeTicker.hpp"
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
    /*
     * As on this tree's pico GamecubeBackend: outside Melee, update slower
     * inputs before waiting for the poll, then read the buttons and run the
     * game mode while the rest of the poll is still arriving. In Melee, the
     * inputs, game mode and output limiter run on the fixed 2 kHz tick instead
     * (MeleeTicker; on the pico, the second core), and the report is built
     * from the limited outputs. Here the ticks are run while waiting for the
     * poll, kept clear of the expected poll time, plus any tick that came due
     * during the poll and reply.
     *
     * (The pico also waits 40 us after the poll starts to sample a little
     * later; we skip that so the reply isn't pushed past the end of the poll.)
     */
    const bool melee = isMelee();

    if (!melee) {
        ScanInputs(InputScanSpeed::SLOW);
        ScanInputs(InputScanSpeed::MEDIUM);
    }

    // Leaves IRQs masked until SendReport() below.
    if (_gamecube.WaitForPollStart(melee) != GcPollStatus::OK) {
        return;
    }

    if (!melee) {
        ScanInputs(InputScanSpeed::FAST);
        UpdateOutputs();
    }

    const OutputState &out = melee ? _finalOutputs : _outputs;

    // Digital outputs - mapping identical to HAL/pico GamecubeBackend.
    _report.a = out.a;
    _report.b = out.b;
    _report.x = out.x;
    _report.y = out.y;
    _report.z = out.buttonR;
    _report.l = out.triggerLDigital;
    _report.r = out.triggerRDigital;
    _report.start = out.start;
    _report.dpad_left = out.dpadLeft | out.select;
    _report.dpad_right = out.dpadRight | out.home;
    _report.dpad_down = out.dpadDown;
    _report.dpad_up = out.dpadUp;

    // Analog outputs.
    _report.stick_x = out.leftStickX;
    _report.stick_y = out.leftStickY;
    _report.cstick_x = out.rightStickX;
    _report.cstick_y = out.rightStickY;
    _report.l_analog = out.triggerLAnalog;
    _report.r_analog = out.triggerRAnalog;

    // Send outputs to the console once the poll has finished.
    if (_gamecube.WaitForPollEnd() == GcPollStatus::OK) {
        _gamecube.SendReport(&_report);
    }

    if (melee) {
        // A tick that came due during the poll and reply (or that was held
        // back because the poll was expected) runs now. At most one extra:
        // the ticker resynchronises if it fell further behind.
        if (melee_ticker_run(micros())) {
            melee_ticker_run(micros());
        }
    }
}
