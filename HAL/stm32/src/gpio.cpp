#include "gpio.hpp"

#include "stdlib.hpp"

namespace gpio {
    void init_pin(uint pin, GpioMode mode) {
        switch (mode) {
            case GpioMode::GPIO_OUTPUT:
                pinMode(pin, OUTPUT);
                break;
            case GpioMode::GPIO_INPUT_PULLUP:
                pinMode(pin, INPUT_PULLUP);
                break;
            case GpioMode::GPIO_INPUT_PULLDOWN:
                pinMode(pin, INPUT_PULLDOWN);
                break;
            case GpioMode::GPIO_INPUT:
            default:
                pinMode(pin, INPUT);
                break;
        }
    }
}
