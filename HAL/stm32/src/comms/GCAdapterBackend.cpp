#include "comms/GCAdapterBackend.hpp"

#include "core/CommunicationBackend.hpp"
#include "core/state.hpp"

#include <string.h>

/*
 * WUP-028 report layout (37 bytes): byte 0 is the 0x21 header, then four 9-byte
 * port slots at offsets 1, 10, 19, 28. Each slot is:
 *   +0 status   (0x10 = wired controller connected)
 *   +1 buttons1 (A 0x01, B 0x02, X 0x04, Y 0x08, D L/R/D/U 0x10/0x20/0x40/0x80)
 *   +2 buttons2 (Start 0x01, Z 0x02, R 0x04, L 0x08)
 *   +3 stick X   +4 stick Y   +5 c-stick X   +6 c-stick Y
 *   +7 trigger L analog   +8 trigger R analog
 * Only port 1 (slot 0) is populated; the rest stay zeroed (status 0x00 = empty).
 */

GCAdapterBackend::GCAdapterBackend(
    InputState &inputs,
    InputSource **input_sources,
    size_t input_source_count,
    GCAdapterUSB &usb
)
    : CommunicationBackend(inputs, input_sources, input_source_count), _usb(usb) {
    memset(_report, 0, sizeof(_report));
    _report[0] = 0x21; // adapter input-report header
}

CommunicationBackendId GCAdapterBackend::BackendId() {
    return COMMS_BACKEND_GAMECUBE;
}

void GCAdapterBackend::SendReport() {
    ScanInputs(InputScanSpeed::SLOW);
    ScanInputs(InputScanSpeed::MEDIUM);

    // Service the USB stack (flush completions -> tx-ready) and drain any host
    // OUT commands before we build the next report.
    _usb.task();
    yield();

    ScanInputs(InputScanSpeed::FAST);
    UpdateOutputs();

    // Port 1 lives at offset 1; ports 2-4 remain zeroed from construction.
    uint8_t *p = &_report[1];

    p[0] = 0x10; // wired GameCube controller connected (type 1)

    // Buttons byte 1: face buttons + D-pad.
    p[1] = (uint8_t)((_outputs.a ? 0x01 : 0) | (_outputs.b ? 0x02 : 0) | (_outputs.x ? 0x04 : 0) |
                     (_outputs.y ? 0x08 : 0) | (_outputs.dpadLeft ? 0x10 : 0) |
                     (_outputs.dpadRight ? 0x20 : 0) | (_outputs.dpadDown ? 0x40 : 0) |
                     (_outputs.dpadUp ? 0x80 : 0));

    // Buttons byte 2: Start, Z (= buttonR), digital R, digital L.
    p[2] = (uint8_t)((_outputs.start ? 0x01 : 0) | (_outputs.buttonR ? 0x02 : 0) |
                     (_outputs.triggerRDigital ? 0x04 : 0) | (_outputs.triggerLDigital ? 0x08 : 0));

    // Analog: raw GameCube 0-255, straight through (up = high; no HID inversion
    // or +1 offset). This is what makes adapter mode "native".
    p[3] = _outputs.leftStickX;
    p[4] = _outputs.leftStickY;
    p[5] = _outputs.rightStickX;
    p[6] = _outputs.rightStickY;
    p[7] = _outputs.triggerLAnalog;
    p[8] = _outputs.triggerRAnalog;

    _usb.sendReport(_report, sizeof(_report));
}
