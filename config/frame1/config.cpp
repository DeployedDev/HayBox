#include "comms/backend_init.hpp"
#include "config_defaults.hpp"
#include "core/CommunicationBackend.hpp"
#include "core/KeyboardMode.hpp"
#include "core/Persistence.hpp"
#include "core/mode_selection.hpp"
#include "core/pinout.hpp"
#include "core/state.hpp"
#include "input/ActiveHighGpioButtonInput.hpp"
#include "reboot.hpp"
#include "stdlib.hpp"

#include <Adafruit_TinyUSB.h>
#include <config.pb.h>

Config config = default_config;

/*
 * Frame1 Heavy button -> GPIO map.
 *
 * Pins were captured and validated live on-device (see milestone 1). The Button
 * enum assignments follow the standard B0XX/HayBox semantics (verified against
 * Melee20Button + GamecubeBackend): e.g. Z == buttonR (BTN_RF3), R ==
 * triggerRDigital (BTN_RF5), L == triggerLDigital (BTN_LF4). Comments give the
 * Frame1's own silk-screen label.
 *
 * The switches are active-HIGH optical sensors, so these are read through
 * ActiveHighGpioButtonInput (pull-downs + non-inverted sense), not the stock
 * active-low GpioButtonInput.
 */
const GpioButtonMapping button_mappings[] = {
    { BTN_LF4, PE5  }, // L
    { BTN_LF3, PE4  }, // Left
    { BTN_LF2, PE3  }, // Down
    { BTN_LF1, PE2  }, // Right

    { BTN_LT1, PC4  }, // MX (Mod X)
    { BTN_LT2, PC5  }, // MY (Mod Y)

    { BTN_MB1, PD2  }, // Start

    { BTN_RF5, PB10 }, // R
    { BTN_RF6, PE14 }, // Y
    { BTN_RF7, PE12 }, // LS (light shield)
    { BTN_RF8, PE10 }, // MS (medium shield)

    { BTN_RF1, PE15 }, // B
    { BTN_RF2, PE13 }, // X
    { BTN_RF3, PE11 }, // Z
    { BTN_RF4, PE9  }, // Up

    { BTN_RT4, PE8  }, // C-Up
    { BTN_RT2, PB0  }, // C-Down
    { BTN_RT3, PB2  }, // C-Left
    { BTN_RT5, PE7  }, // C-Right
    { BTN_RT1, PB1  }, // A
};
const size_t button_count = sizeof(button_mappings) / sizeof(GpioButtonMapping);

/*
 * Console (wired-protocol) pins. ALL are PLACEHOLDERS - change them to the free
 * pads actually exposed on the board, then wire each console's lines + ground +
 * power. joybus_data (PA0) carries GameCube/N64 (single wire); nes_data/clock/
 * latch (PA1/PA2/PA3) carry NES/SNES (shift register). mux / nunchuk unused.
 */
const Pinout pinout = {
    .joybus_data = PA0,
    .nes_data = PA1,
    .nes_clock = PA2,
    .nes_latch = PA3,
    .mux = -1,
    .nunchuk_detect = -1,
    .nunchuk_sda = -1,
    .nunchuk_scl = -1,
};

CommunicationBackend **backends = nullptr;
size_t backend_count;
KeyboardMode *current_kb_mode = nullptr;

void setup() {
    // STM32duino is a "non-built-in" TinyUSB core: unlike the RP2040 core, it does
    // not bring up the TinyUSB device stack before setup(), so we must. This runs
    // our F303 TinyUSB_Port_InitDevice() (USB clock/pins/IRQ) and tusb_init(). It
    // has to happen before the backends register their HID interface, because the
    // device begin() clears the configuration descriptor.
    TinyUSB_Device_Init(0);

    static InputState inputs;

    // Create GPIO input source and read it once so button-holds are available
    // for backend/mode selection in initialize_backends().
    static ActiveHighGpioButtonInput gpio_input(button_mappings, button_count);
    gpio_input.UpdateInputs(inputs);

    /* DFU note: the Frame1's protected bootloader enters DFU when Up is held at
     * power-on (a hardware check at 0x08000000). There is no documented software
     * trigger, so there is no bootloader button-hold handled here - the user
     * holds Up while plugging in to reflash. */

    // Attempt to load config, or write default config to flash if failed to load config.
    if (!persistence.LoadConfig(config)) {
        persistence.SaveConfig(config);
    }

    // Single input source; the active backend scans it each SendReport().
    static InputSource *input_sources[] = {
        &gpio_input,
    };
    size_t input_source_count = sizeof(input_sources) / sizeof(InputSource *);

    backend_count =
        initialize_backends(backends, inputs, input_sources, input_source_count, config, pinout);

    setup_mode_activation_bindings(config.game_mode_configs, config.game_mode_configs_count);

    // The backends have registered their USB (HID) interface now, so re-present to
    // the host to force a clean enumeration of the full descriptor set.
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();
}

void loop() {
    // Non-built-in TinyUSB core: pump the device task every iteration.
    TinyUSB_Device_Task();

    select_mode(backends, backend_count, config);

    for (size_t i = 0; i < backend_count; i++) {
        backends[i]->SendReport();
    }

    if (current_kb_mode != nullptr) {
        current_kb_mode->SendReport(backends[0]->GetInputs());
    }

    // Non-built-in TinyUSB core: Adafruit's CDC write() only fills a FIFO and
    // relies on the core to flush it to the host (arduino-pico does this behind
    // the scenes). Here we must do it ourselves, or the Configurator's replies
    // and the B0XX input viewer's stream sit in the FIFO and never go out.
    TinyUSB_Device_FlushCDC();
}
