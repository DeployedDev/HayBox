/*
 * TinyUSB device configuration for the Frame1 (STM32F303VE) port.
 *
 * The F303xE USB peripheral is the "v2" full-speed device core with a linear
 * (x1) packet memory - the same peripheral we validated at the libopencm3
 * proof-of-concept stage. TinyUSB's dcd_stm32_fsdev driver handles it; STM32duino
 * selects that driver from CFG_TUSB_MCU below. Device-only, composite CDC + HID
 * (TUCompositeHID packs the gamepad and keyboard into a single HID interface).
 */

#ifndef _TUSB_CONFIG_STM32_H_
#define _TUSB_CONFIG_STM32_H_

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------
// COMMON CONFIGURATION
//--------------------------------------------------------------------

#ifndef CFG_TUSB_MCU
#define CFG_TUSB_MCU OPT_MCU_STM32F3
#endif

#define CFG_TUSB_OS OPT_OS_NONE

#define CFG_TUSB_RHPORT0_MODE OPT_MODE_DEVICE

// Enable device stack
#define CFG_TUD_ENABLED 1

#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG 0
#endif

#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN TU_ATTR_ALIGNED(4)

//--------------------------------------------------------------------
// Device Configuration
//--------------------------------------------------------------------

#define CFG_TUD_ENDPOINT0_SIZE 64

#define CFG_TUD_CDC 1
#define CFG_TUD_MSC 0
#define CFG_TUD_HID 1
#define CFG_TUD_MIDI 0
#define CFG_TUD_VENDOR 0

// CDC FIFO size of TX and RX
#define CFG_TUD_CDC_RX_BUFSIZE 256
#define CFG_TUD_CDC_TX_BUFSIZE 256

// HID buffer size - sufficient to hold report ID (if any) + data
#define CFG_TUD_HID_EP_BUFSIZE 64

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_CONFIG_STM32_H_ */
