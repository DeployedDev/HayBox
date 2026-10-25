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
 *
 * Reads use digitalReadFast() on a pin name resolved once at startup, rather
 * than digitalRead() (which looks the pin up on every call and is slow on this
 * core). On the console backends the buttons are read between the console's
 * poll and our reply, as on the pico, so the scan must be quick - and the pico's
 * own GPIO reads are single-register reads too.
 */
class ActiveHighGpioButtonInput : public GpioButtonInput {
  public:
    ActiveHighGpioButtonInput(const GpioButtonMapping *button_mappings, size_t button_count)
        : GpioButtonInput(button_mappings, button_count) {
        _pins = new PinName[_button_count];
        // The base constructor configured these as pull-ups; switch them to
        // pull-downs so an idle (undriven) optical line reads LOW.
        for (size_t i = 0; i < _button_count; i++) {
            gpio::init_pin(_button_mappings[i].pin, gpio::GpioMode::GPIO_INPUT_PULLDOWN);
            _pins[i] = digitalPinToPinName(_button_mappings[i].pin);
        }
    }

    void UpdateInputs(InputState &inputs) override {
        for (size_t i = 0; i < _button_count; i++) {
            // Active-high: a HIGH reading means the button is pressed.
            set_button(
                inputs.buttons,
                _button_mappings[i].button,
                digitalReadFast(_pins[i]) == HIGH
            );
        }
    }

  private:
    PinName *_pins;  // resolved once in the constructor
};

#endif
