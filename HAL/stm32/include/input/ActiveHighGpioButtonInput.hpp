#ifndef _INPUT_ACTIVEHIGHGPIOBUTTONINPUT_HPP
#define _INPUT_ACTIVEHIGHGPIOBUTTONINPUT_HPP

#include "gpio.hpp"
#include "input/GpioButtonInput.hpp"
#include "stdlib.hpp"
#include "util/state_util.hpp"

/*
 * GpioButtonInput variant for boards whose switches are active-HIGH.
 *
 * The stock (shared) GpioButtonInput targets active-low mechanical switches: it
 * configures each pin as INPUT_PULLUP and treats a LOW reading as "pressed".
 * The Frame1 Heavy uses optical switches that idle LOW and drive HIGH when
 * pressed, so we:
 *   1. re-initialize every button pin with a pull-down (validated on-device),
 *      and
 *   2. report "pressed" when the pin reads HIGH (no inversion).
 *
 * Everything else (the button->pin table, InputSource plumbing) is inherited
 * unchanged from GpioButtonInput.
 */
class ActiveHighGpioButtonInput : public GpioButtonInput {
  public:
    ActiveHighGpioButtonInput(const GpioButtonMapping *button_mappings, size_t button_count)
        : GpioButtonInput(button_mappings, button_count) {
        // The base constructor configured these as pull-ups; switch them to
        // pull-downs so an idle (undriven) optical line reads LOW.
        for (size_t i = 0; i < _button_count; i++) {
            gpio::init_pin(_button_mappings[i].pin, gpio::GpioMode::GPIO_INPUT_PULLDOWN);
        }
    }

    void UpdateInputs(InputState &inputs) override {
        for (size_t i = 0; i < _button_count; i++) {
            // Active-high: a HIGH reading means the button is pressed.
            set_button(
                inputs.buttons,
                _button_mappings[i].button,
                gpio::read_digital(_button_mappings[i].pin)
            );
        }
    }
};

#endif
