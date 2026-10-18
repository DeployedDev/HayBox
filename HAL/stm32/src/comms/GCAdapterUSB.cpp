#include "comms/GCAdapterUSB.hpp"

#include <string.h>

/*
 * Single vendor instance. TinyUSB's vendor-class callbacks are global (not bound
 * to a C++ object), so the send-pacing flag lives here at file scope. It starts
 * true so the first report can be queued as soon as the host is mounted.
 */
static volatile bool s_tx_ready = true;

GCAdapterUSB::GCAdapterUSB() {}

bool GCAdapterUSB::begin() {
    return TinyUSBDevice.addInterface(*this);
}

uint16_t GCAdapterUSB::getInterfaceDescriptor(uint8_t, uint8_t *buf, uint16_t bufsize) {
    // 9-byte interface descriptor + two 7-byte endpoint descriptors.
    uint8_t const desc_len = 9 + 7 + 7;
    if (buf == NULL) {
        return desc_len;
    }

    uint8_t const itfnum = TinyUSBDevice.allocInterface(1);
    // Match the real WUP-028's endpoint addresses exactly: IN 0x81, OUT 0x02.
    // Modern Dolphin scans the descriptor by direction, but some builds and
    // tools hardcode these, and we are the sole interface in adapter mode so
    // there is nothing to collide with. (allocEndpoint would hand out OUT 0x01.)
    uint8_t const ep_in = 0x81;
    uint8_t const ep_out = 0x02;

    uint8_t const desc[] = {
        // Interface: vendor-specific (0xFF), 2 endpoints.
        9, TUSB_DESC_INTERFACE, itfnum, 0x00, 0x02, TUSB_CLASS_VENDOR_SPECIFIC, 0x00, 0x00, _strid,
        // Interrupt IN endpoint - 37-byte input reports, 1 ms poll interval.
        7, TUSB_DESC_ENDPOINT, ep_in, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(64), 1,
        // Interrupt OUT endpoint - 0x13 start / 0x11 rumble commands, 1 ms.
        7, TUSB_DESC_ENDPOINT, ep_out, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(64), 1,
    };

    if (bufsize < sizeof(desc)) {
        return 0;
    }
    memcpy(buf, desc, sizeof(desc));
    return sizeof(desc);
}

void GCAdapterUSB::sendReport(const uint8_t *report, uint16_t len) {
    if (!tud_vendor_mounted() || !s_tx_ready) {
        return;
    }
    if (tud_vendor_write_available() < len) {
        return;
    }
    tud_vendor_write(report, len);
    tud_vendor_write_flush();
    s_tx_ready = false;
}

void GCAdapterUSB::task() {
    while (tud_vendor_available()) {
        uint8_t cmd[64];
        tud_vendor_read(cmd, sizeof(cmd));
        // 0x13 = start polling (we already stream when mounted); 0x11 = rumble
        // (no motor). Nothing to do but consume the bytes.
    }
}

// TinyUSB vendor-class callbacks (global, single instance).
extern "C" {

void tud_vendor_tx_cb(uint8_t itf, uint32_t sent_bytes) {
    (void)itf;
    (void)sent_bytes;
    s_tx_ready = true;
}

void tud_vendor_rx_cb(uint8_t itf, uint8_t const *buffer, uint32_t bufsize) {
    (void)itf;
    (void)buffer;
    (void)bufsize;
    // OUT commands are drained in GCAdapterUSB::task(); nothing to do here.
}
}
