#ifndef _COMMS_MELEETICKER_HPP
#define _COMMS_MELEETICKER_HPP

#include <stdint.h>

/*
 * Fixed-rate scheduler for this tree's ruleset-compliant Melee pipeline on the
 * Frame1 (single core).
 *
 * On the RP2040 the second core scans the buttons, runs the game mode and the
 * output limiter (CommunicationBackend::LimitOutputs) at exactly 2 kHz, while
 * the first core only ships the limited outputs. The limiter counts calls as
 * time, so it must be called at a steady rate. The F303 has one core, so the
 * board config registers its "tick" (scan + game mode + limit for every
 * backend) here, and whoever is in control calls melee_ticker_run():
 *   - the main loop, between (non-blocking) USB reports;
 *   - the GameCube backend, while it waits for the console's poll (see
 *     GamecubeConsole::WaitForPollStart), which would otherwise block the main
 *     loop for up to a frame.
 *
 * melee_ticker_run() runs at most one tick per call, keeps ticks on a 500 us
 * grid, and if it ever falls more than a whole period behind it resynchronises
 * instead of running catch-up ticks back-to-back (which would skew the
 * limiter's time base).
 */

#define MELEE_TICK_INTERVAL_US 500

typedef void (*melee_tick_fn_t)();

// Register the tick function (call once, in setup()).
void melee_ticker_init(melee_tick_fn_t tick);

// Microseconds until the next tick is due; <= 0 means due now.
int32_t melee_ticker_us_until_due(uint32_t now_us);

// Run one tick if one is due. Returns true if a tick ran.
bool melee_ticker_run(uint32_t now_us);

// Longest tick seen so far, in microseconds (used to keep ticks clear of an
// expected GameCube poll).
uint32_t melee_ticker_max_tick_us();

#endif
