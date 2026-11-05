#ifndef _COMMS_SHIFTREGISTERIO_HPP
#define _COMMS_SHIFTREGISTERIO_HPP

#include <Arduino.h>
#include <stdint.h>

/*
 * Shared NES/SNES shift-register transport (device side) for the STM32
 * (Frame1).  [not tested on hardware]
 *
 * STM32 stand-in for the bit-level half of JonnyHaystack/nes-pio (RP2040 PIO).
 * The console drives LATCH and CLOCK (both idle high, pulse low except LATCH
 * which pulses high); the controller presents its button bits on DATA,
 * active-low (pressed = line driven low, released = hi-Z high). On a latch the
 * controller loads state and presents bit 0; each CLOCK pulse advances a bit.
 *
 * DATA is open-drain; CLOCK and LATCH are inputs. HARDWARE: the Frame1 has no
 * NES/SNES lines, so its pinout sets them to -1 (as the reference pico board
 * does) and this transport is built but unused.
 */
class ShiftRegisterIO {
  public:
    ShiftRegisterIO(uint8_t data_pin, uint8_t clock_pin, uint8_t latch_pin);

    void Init();

    // Wait for the next latch, then shift out `nbits` bits (bit 0 first),
    // active-low. Returns false if no latch arrived within a coarse timeout.
    bool RespondToPoll(const bool *pressed, uint8_t nbits);

  private:
    uint8_t _data_pin, _clock_pin, _latch_pin;
    PinName _data, _clock, _latch;
};

#endif
