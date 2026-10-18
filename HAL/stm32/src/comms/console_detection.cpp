#include "comms/console_detection.hpp"

#include "core/pinout.hpp"

#include <config.pb.h>

/*
 * The Frame1 is a USB-only board - there are no GameCube/N64/NES data lines to
 * probe, so there is nothing to auto-detect. Returning UNSPECIFIED makes
 * initialize_backends() fall through to the configured default USB backend.
 *
 * When the retro (joybus/clock-latch) backends are eventually ported, this is
 * where their line-detection logic would go.
 */
CommunicationBackendId detect_console(const Pinout &pinout) {
    (void)pinout;
    return COMMS_BACKEND_UNSPECIFIED;
}
