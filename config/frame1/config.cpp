#include "comms/MeleeTicker.hpp"
#include "comms/backend_init.hpp"
#include "config_defaults.hpp"
#include "core/CommunicationBackend.hpp"
#include "core/KeyboardMode.hpp"
#include "core/Persistence.hpp"
#include "core/mode_selection.hpp"
#include "core/pinout.hpp"
#include "core/state.hpp"
#include "input/DebouncedGpioButtonInput.hpp"
#include "reboot.hpp"
#include "stdlib.hpp"

#include <Adafruit_TinyUSB.h>
#include <config.pb.h>

Config config = default_config;

/*
 * Frame1 button -> GPIO map (shared by the Heavy and the Light).
 *
 * Pins were captured live on-device and validated on both the Heavy and the
 * Light, and match the pin set configured by Frame1's stock firmware (v2.3.1),
 * which is a single build for both models. The Button enum assignments follow
 * the standard B0XX/HayBox semantics (verified against Melee20Button +
 * GamecubeBackend): e.g. Z == buttonR (BTN_RF3), R == triggerRDigital
 * (BTN_RF5), L == triggerLDigital (BTN_LF4). Comments give the Frame1's own
 * silk-screen label.
 *
 * Switch polarity differs by model - see frame1_buttons_active_high().
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
 * Heavy vs Light. As in Frame1's stock firmware, PB12 and PB13 are pull-up
 * strap inputs read before the buttons; if either reads high, the stock
 * firmware inverts every button reading.
 *
 * Measured on a Heavy: both straps read LOW (even with pull-ups), and every
 * button pin idles LOW and reads HIGH when pressed, with pull-ups or pull-downs
 * alike (the optical switches drive the pin). So both straps low means the
 * Heavy (active-high); a strap high means the Light, whose mechanical switches
 * pull the pin LOW when pressed (active-low; confirmed on a Light). Button pins
 * use pull-ups on both models, as in stock.
 */
static bool frame1_buttons_active_high() {
    pinMode(PB12, INPUT_PULLUP);
    pinMode(PB13, INPUT_PULLUP);
    delayMicroseconds(50);
    return digitalRead(PB12) == LOW && digitalRead(PB13) == LOW;
}

/*
 * Console (wired-protocol) pins. joybus_data = PD3 is the GameCube/N64 single-
 * wire data line, found and validated on hardware. The Frame1's USB-C port
 * follows the Retro-C pinout (data on A11/B11), so a Retro-C style USB-C to
 * GameCube or N64 cable connects it with no wiring. The Frame1 has no NES/SNES
 * lines; the nes_* fields are left unset, as in this tree's pico config (see
 * console_detection.cpp). mux / nunchuk unused.
 */
const Pinout pinout = {
    .joybus_data = PD3,
    .mux = -1,
    .nunchuk_detect = -1,
    .nunchuk_sda = -1,
    .nunchuk_scl = -1,
};

CommunicationBackend **backends = nullptr;
size_t backend_count;
KeyboardMode *current_kb_mode = nullptr;

/*
 * One step of the ruleset-compliant Melee pipeline, run at 2 kHz by
 * MeleeTicker: scan the buttons, run the game mode, and run the output limiter
 * (travel time, SOCD/fuzzing limits). The backends then ship the limited
 * outputs. See the comment in loop().
 */
static void melee_tick() {
    for (size_t i = 0; i < backend_count; i++) {
        backends[i]->ScanInputs();
        backends[i]->UpdateOutputs();
        // The limiter's time unit is 4 us.
        backends[i]->LimitOutputs(MELEE_TICK_INTERVAL_US / 4);
    }
}

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
    static DebouncedGpioButtonInput<button_count> gpio_input(
        button_mappings,
        frame1_buttons_active_high()
    );
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

    melee_ticker_init(&melee_tick);

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

    /*
     * Ruleset-compliant Melee pipeline.
     *
     * On the RP2040 the second core scans the buttons, runs the game mode and
     * the output limiter (travel time, SOCD/fuzzing limits) at exactly 2 kHz,
     * while the first core only ships the limited outputs. The F303 has one
     * core, so the same 2 kHz cadence is kept on a fixed 500 us schedule by
     * MeleeTicker (melee_tick() above): the limiter counts calls as time, so it
     * must be called at a steady rate. The USB backends' SendReport() is
     * non-blocking in Melee mode (see HAL/stm32/src/comms) so it can never stall
     * this schedule; the GameCube backend runs the ticks itself while it waits
     * for the console's poll. Outside Melee the backends scan and compute
     * inside SendReport(), as on the pico.
     */
    bool is_melee = false;
    for (size_t i = 0; i < backend_count; i++) {
        if (backends[i]->isMelee()) {
            is_melee = true;
        }
    }
    if (is_melee) {
        melee_ticker_run(micros());
    }

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
