/*
 * TinyUSB Arduino port for the STM32F303 (USB FS "device" / fsdev peripheral).
 *
 * This replaces Adafruit_TinyUSB_Arduino's stock
 * src/arduino/ports/stm32/Adafruit_TinyUSB_stm32.cpp, which targets ONLY the
 * USB-OTG peripheral found on F4/F7/H7 parts (OTG_FS registers, GPIO_AF10_OTG_FS,
 * OTG_FS_IRQn, and a hard #include of stm32f4xx_hal.h). The STM32F303 has the
 * simpler USB FS *device* controller instead - TinyUSB's dcd_stm32_fsdev driver,
 * selected by CFG_TUSB_MCU == OPT_MCU_STM32F3 - on PA11/PA12 (AF14), with the
 * CAN-shared USB_LP interrupt and an on-chip DP pull-up (BCDR.DPPU, enabled by
 * the fsdev driver from dcd_init()). This is the same peripheral we validated at
 * the libopencm3 proof-of-concept stage, and mirrors TinyUSB's own F3 BSP.
 *
 * It is copied over the installed library file at build time by
 * builder_scripts/patch_tinyusb_stm32.py (the Adafruit library lives outside the
 * repo, so it cannot be edited in place).
 */

#include "tusb_option.h"

#if (defined(ARDUINO_ARCH_STM32) ||                                            \
     defined(ARDUINO_ARCH_ARDUINO_CORE_STM32)) &&                              \
    CFG_TUD_ENABLED

#define USE_HAL_DRIVER
#include "Arduino.h"
#include "arduino/Adafruit_TinyUSB_API.h"
#include "tusb.h"

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+
extern "C" {

// On STM32F303 the USB low-priority interrupt line is shared with CAN RX0.
void USB_LP_CAN_RX0_IRQHandler(void) { tud_int_handler(0); }

void yield(void) {
  tud_task();
  if (tud_cdc_connected()) {
    tud_cdc_write_flush();
  }
}

} // extern "C"

//--------------------------------------------------------------------+
// Porting API
//--------------------------------------------------------------------+
void TinyUSB_Port_InitDevice(uint8_t rhport) {
  // Enable GPIOA + USB peripheral clocks.
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_USB_CLK_ENABLE();

  // PA11 = USB_DM, PA12 = USB_DP, alternate function 14 on the F3 USB device.
  GPIO_InitTypeDef GPIO_InitStruct = {};
  GPIO_InitStruct.Pin = GPIO_PIN_11 | GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF14_USB;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  // Enable the USB low-priority interrupt.
  NVIC_SetPriority(USB_LP_CAN_RX0_IRQn, 0);
  NVIC_EnableIRQ(USB_LP_CAN_RX0_IRQn);

  // Initialize the TinyUSB device stack. The fsdev driver enables the on-chip
  // D+ pull-up (BCDR.DPPU) from dcd_init(), so no external pull-up control is
  // needed.
  const tusb_rhport_init_t rh_init = {
      .role = TUSB_ROLE_DEVICE,
      .speed = TUSB_SPEED_FULL,
  };
  tusb_init(rhport, &rh_init);
}

void TinyUSB_Port_EnterDFU(void) {
  // The Frame1's DFU bootloader is entered by holding Up at power-on (a check in
  // its protected bootloader at 0x08000000); there is no documented software
  // trigger, so we just reset and the user holds Up to drop into DFU.
  NVIC_SystemReset();
}

uint8_t TinyUSB_Port_GetSerialNumber(uint8_t serial_id[16]) {
  // STM32F3 96-bit unique device ID base (RM0316).
  volatile uint32_t *uid = (volatile uint32_t *)0x1FFFF7AC;
  uint32_t *serial_32 = (uint32_t *)serial_id;
  serial_32[0] = uid[0];
  serial_32[1] = uid[1];
  serial_32[2] = uid[2];
  return 12;
}

#endif
