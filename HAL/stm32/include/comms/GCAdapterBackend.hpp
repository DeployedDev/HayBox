#ifndef _COMMS_GCADAPTERBACKEND_HPP
#define _COMMS_GCADAPTERBACKEND_HPP

#include "comms/GCAdapterUSB.hpp"
#include "core/CommunicationBackend.hpp"
#include "core/state.hpp"

#include <stddef.h>
#include <stdint.h>

/*
 * Emulated "GameCube Adapter for Wii U" backend.
 *
 * Packs the active mode's OutputState into the adapter's 37-byte report (port 1
 * = a connected wired controller, ports 2-4 empty) and streams it over the
 * GCAdapterUSB vendor interface. The analog values pass through untouched, so
 * Dolphin's adapter mode receives exact GameCube coordinates - no HID axis
 * scaling, inversion or Range tweak.
 */
class GCAdapterBackend : public CommunicationBackend {
  public:
    GCAdapterBackend(
        InputState &inputs,
        InputSource **input_sources,
        size_t input_source_count,
        GCAdapterUSB &usb
    );

    CommunicationBackendId BackendId();
    void SendReport();

  private:
    GCAdapterUSB &_usb;
    uint8_t _report[37];
};

#endif
