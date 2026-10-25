#ifndef _INPUT_DEBOUNCEDGPIOBUTTONINPUT_HPP
#define _INPUT_DEBOUNCEDGPIOBUTTONINPUT_HPP

#include "core/state.hpp"
#include "input/GpioButtonInput.hpp"
#include "input/debounce.hpp"
#include "stdlib.hpp"
#include "util/state_util.hpp"

/*
 * STM32 counterpart of HAL/pico/include/input/DebouncedGpioButtonInput.hpp:
 * GPIO buttons with the pico's 5 ms debounce. Pins are configured with pull-ups
 * by the GpioButtonInput base, as on the pico.
 *
 * Port additions:
 *   - `active_high`: the stock HayBox wiring is active-low (switch to ground).
 *     Pass true for boards whose switches drive the pin HIGH when pressed.
 *   - Reads use digitalReadFast() on pin names resolved once at construction,
 *     not digitalRead() (which looks the pin up on every call and is slow on
 *     this core). On the console backends the buttons are read between the
 *     console's poll and our reply, so the scan must be quick; the pico's own
 *     GPIO reads are single-register reads too.
 */
template <size_t button_count> class DebouncedGpioButtonInput : public GpioButtonInput {
  public:
    DebouncedGpioButtonInput(
        const GpioButtonMapping button_mappings[button_count],
        bool active_high = false,
        uint32_t debounce_period_ms = 5
    )
        : GpioButtonInput(button_mappings, button_count) {
        _active_high = active_high;
        _debounce_period_ms = debounce_period_ms;
        for (size_t i = 0; i < button_count; i++) {
            _pins[i] = digitalPinToPinName(button_mappings[i].pin);
        }
    }

    void UpdateInputs(InputState &inputs) override {
        for (size_t i = 0; i < button_count; i++) {
            const bool high = digitalReadFast(_pins[i]) == HIGH;
            UpdateButtonState(inputs, i, high == _active_high);
        }
    }

  private:
    PinName _pins[button_count];
    DebounceState _debounce_state[button_count];
    bool _active_high;
    uint32_t _debounce_period_ms;

    void UpdateButtonState(InputState &inputs, size_t button_mapping_index, bool pressed) {
        bool state_changed = update_debounce_state(
            _debounce_state[button_mapping_index],
            pressed,
            _debounce_period_ms
        );
        if (state_changed) {
            set_button(inputs.buttons, _button_mappings[button_mapping_index].button, pressed);
        }
    }
};

#endif
