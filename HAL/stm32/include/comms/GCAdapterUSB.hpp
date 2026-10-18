#ifndef _COMMS_GCADAPTERUSB_HPP
#define _COMMS_GCADAPTERUSB_HPP

#include <Adafruit_TinyUSB.h>

#include <stddef.h>
#include <stdint.h>

/*
 * USB device interface that emulates Nintendo's GameCube Controller Adapter for
 * Wii U (WUP-028, USB 057e:0337), so Dolphin's "GameCube Adapter for Wii U"
 * input mode reads the Frame1 natively: exact 0-255 GameCube values, with none
 * of the HID-gamepad mapping, range or gate processing the DInput path needs.
 *
 * It presents a single vendor-specific interface (class 0xFF) with two interrupt
 * endpoints:
 *   - IN : streams the adapter's 37-byte input report on each host poll.
 *   - OUT: receives the host's commands (0x13 "start polling", 0x11 rumble).
 *
 * It runs on the custom F303 fsdev TinyUSB port through TinyUSB's bundled
 * vendor-device class (CFG_TUD_VENDOR). There is a single instance, so the
 * global vendor-class callbacks route to file-scope state in the .cpp.
 */
class GCAdapterUSB : public Adafruit_USBD_Interface {
  public:
    GCAdapterUSB();

    // Register the vendor interface with the TinyUSB device. Call after
    // TinyUSBDevice.clearConfiguration()/setID() and before the re-attach.
    bool begin();

    // Adafruit_USBD_Interface: emit the vendor interface + 2 interrupt endpoints.
    uint16_t getInterfaceDescriptor(uint8_t itfnum, uint8_t *buf, uint16_t bufsize) override;

    // Queue the latest 37-byte input report for the host's next interrupt poll.
    // No-ops until the previous report has actually been sent, so the host always
    // gets the freshest state and reports never back up.
    void sendReport(const uint8_t *report, uint16_t len);

    // Drain host OUT commands so the endpoint keeps accepting them. We have no
    // rumble motor, so 0x11 is read and discarded; 0x13 only asks us to stream,
    // which we already do whenever mounted.
    void task();
};

#endif
