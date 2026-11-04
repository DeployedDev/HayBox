#include "comms/MeleeTicker.hpp"

#include <Arduino.h>

static melee_tick_fn_t s_tick = nullptr;
static bool s_started = false;
static uint32_t s_next_us = 0;
static uint32_t s_max_tick_us = 0;

void melee_ticker_init(melee_tick_fn_t tick) {
    s_tick = tick;
}

int32_t melee_ticker_us_until_due(uint32_t now_us) {
    if (!s_started) {
        return 0;
    }
    return (int32_t)(s_next_us - now_us);
}

bool melee_ticker_run(uint32_t now_us) {
    if (s_tick == nullptr) {
        return false;
    }
    if (!s_started) {
        s_next_us = now_us;
        s_started = true;
    }
    if ((int32_t)(now_us - s_next_us) < 0) {
        return false;
    }

    s_next_us += MELEE_TICK_INTERVAL_US;
    if ((int32_t)(now_us - s_next_us) >= (int32_t)MELEE_TICK_INTERVAL_US) {
        // Fell more than a whole period behind (e.g. right after a mode switch):
        // resynchronise instead of running catch-up ticks back-to-back.
        s_next_us = now_us + MELEE_TICK_INTERVAL_US;
    }

    const uint32_t t0 = micros();
    s_tick();
    const uint32_t dt = micros() - t0;
    if (dt > s_max_tick_us) {
        s_max_tick_us = dt;
    }
    return true;
}

uint32_t melee_ticker_max_tick_us() {
    return s_max_tick_us;
}
