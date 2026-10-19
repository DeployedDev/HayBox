#include "comms/console_detection.hpp"

#include "comms/GamecubeConsole.hpp"
#include "core/pinout.hpp"

#include <config.pb.h>

/*
 * Frame1 console auto-detection.
 *
 * The board is USB-only until a console data line is physically wired. When a
 * GameCube/Wii is driving the Joybus line (pinout.joybus_data), we select the
 * GameCube backend; otherwise we return UNSPECIFIED so initialize_backends()
 * falls through to the default USB backend (XInput).
 *
 * N64 shares the Joybus line and NES/SNES use the nes_* lines; their detection
 * (and the pico's USB-connected fast-path) is added as those backends land.
 */
CommunicationBackendId detect_console(const Pinout &pinout) {
    GamecubeConsole gamecube(pinout.joybus_data);
    gamecube.Init();
    if (gamecube.Detect()) {
        return COMMS_BACKEND_GAMECUBE;
    }

    return COMMS_BACKEND_UNSPECIFIED;
}
