#ifndef _COMMS_JOYBUSIO_HPP
#define _COMMS_JOYBUSIO_HPP

#include <Arduino.h>
#include <stdint.h>

/*
 * Shared single-wire Joybus transport for the STM32 (Frame1).  [unvalidated]
 *
 * GameCube and N64 both sit on this. It is the STM32 stand-in for the bit-level
 * half of JonnyHaystack/joybus-pio (which the RP2040 does in PIO): an open-drain
 * GPIO, DWT cycle-timed 4 us bit cells (0 = ~3us low +1us high; 1 = ~1us low
 * +3us high; RX sampled ~2us after the falling edge).
 *
 * Interrupt policy: this transport never touches the interrupt mask. The caller
 * (the console classes) masks IRQs around a command/response burst so the bit
 * timing is jitter-free, and keeps them masked between a WaitForPoll()==OK and
 * the matching SendReport().
 *
 * The us->cycle constants and the fast-GPIO path are the first things to check
 * on a logic analyser on real hardware.
 */
class JoybusIO {
  public:
    explicit JoybusIO(uint8_t data_pin);

    void Init();                 // open-drain pin + DWT cycle counter
    bool ReadLine();             // true = line high (released/idle)

    // Send `len` bytes MSB-first. Caller masks IRQs.
    void SendBytes(const uint8_t *data, uint8_t len);
    // Read `len` bytes MSB-first; false on inter-edge timeout. Caller masks IRQs.
    bool ReadBytes(uint8_t *data, uint8_t len);
    // ~1 us low + release, closing a response.
    void SendStopBit();

    // Detection helper: within `budget_ms`, watch the line for a command whose
    // opcode equals `wanted` (e.g. 0x40 for a GameCube poll, 0x01 for N64) and
    // return true if seen. Masks IRQs only for each short command read.
    bool SniffFor(uint8_t wanted, uint32_t budget_ms);

    // Active-detection pin control. BeginListen() sets up timing and configures
    // the pin as a pulled-up input: an idle or unconnected pin reads a stable
    // HIGH (so a floating candidate pin is not mistaken for bus traffic) while
    // reads still work. EnableDrive() switches to open-drain so responses can be
    // sent; call it once real traffic is seen. Reading works in either mode.
    void BeginListen();
    void EnableDrive();

  private:
    uint8_t _pin;
    PinName _pn;
    uint32_t _c1, _c2, _c3;  // 1/2/3 us in CPU cycles (set in Init)
};

#endif
