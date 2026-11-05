#include "reboot.hpp"

#include "stdlib.hpp"

void reboot_firmware() {
    NVIC_SystemReset();
}

void reboot_bootloader() {
    /* The Frame1's DFU bootloader is entered by holding the Up button at
     * power-on (a hardware check in its protected bootloader at 0x08000000).
     * There is no documented software trigger for it yet, so for now we just
     * reset the MCU; the user holds Up to drop into DFU. If a software entry
     * mechanism is found later (e.g. a magic value the bootloader checks),
     * wire it in here before the reset. */
    NVIC_SystemReset();
}
