#ifndef _DEBOUNCE_HPP
#define _DEBOUNCE_HPP

#include "stdlib.hpp"

/*
 * Port of HAL/pico/include/input/debounce.hpp. Same behaviour: a change is
 * reported immediately, then further changes on that button are ignored for
 * the debounce period. The pico SDK's absolute_time_t is replaced with micros()
 * (wrap-safe signed comparison).
 */

typedef struct _DebounceState {
    uint32_t locked_until_us = 0;
    bool locked = false;
    bool pressed = false;
} DebounceState;

inline bool update_debounce_state(
    DebounceState &debounce_state,
    bool current_reading,
    uint32_t debounce_period_ms
) {
    const uint32_t now = micros();

    // If currently locked out, do nothing.
    if (debounce_state.locked && (int32_t)(now - debounce_state.locked_until_us) < 0) {
        return false;
    }
    debounce_state.locked = false;

    // If latest reading differs from the last known state, update the value and timeout.
    if (current_reading != debounce_state.pressed) {
        debounce_state.pressed = current_reading;
        debounce_state.locked_until_us = now + debounce_period_ms * 1000UL;
        debounce_state.locked = true;
        return true;
    }

    return false;
}

#endif
