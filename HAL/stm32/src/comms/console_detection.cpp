#include "comms/console_detection.hpp"

#include "comms/GamecubeConsole.hpp"
#include "comms/N64Console.hpp"
#include "core/pinout.hpp"

#include <config.pb.h>

/*
 * Frame1 console auto-detection.
 *
 * The board is USB-only until a console data line is physically wired. We watch
 * the Joybus line (pinout.joybus_data) and pick GameCube or N64 by which poll
 * opcode the console sends (0x40 vs 0x01); otherwise we return UNSPECIFIED so
 * initialize_backends() falls through to the default USB backend (XInput).
 *
 * NES/SNES detection (on the nes_* lines) and the pico's USB-connected
 * fast-path are added as those backends land.
 */
CommunicationBackendId detect_console(const Pinout &pinout) {
    GamecubeConsole gamecube(pinout.joybus_data);
    gamecube.Init();
    if (gamecube.Detect()) {
        return COMMS_BACKEND_GAMECUBE;
    }

    N64Console n64(pinout.joybus_data);
    n64.Init();
    if (n64.Detect()) {
        return COMMS_BACKEND_N64;
    }

    return COMMS_BACKEND_UNSPECIFIED;
}
