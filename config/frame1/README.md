# Frame1 Heavy (STM32F303VE)

USB (DInput) port of HayBox for the Frame1 Heavy leverless controller.

## Build & flash

```sh
pio run -e frame1
```

The controller uses a protected DFU bootloader at `0x08000000`; the app lives
above it at `0x08020000`. To flash:

1. Hold **Up** and plug in the USB cable (this enters DFU — it enumerates as
   `0483:df11`).
2. Flash the firmware:

   ```sh
   dfu-util -a 0 -s 0x08020000 -D .pio/build/frame1/firmware.bin
   ```

Recovery is always hold-**Up** at power-on, so a bad flash cannot brick the
board.

## Using with Dolphin (USB)

The board enumerates as **`Adafruit FRAME1`**. Select it as the device, set it
to **Standard Controller**, and map the buttons as normal. Two settings are not
obvious:

### Analog R trigger (light / medium shield)

Bind **R-Analog** to the inverted axis expression and set that input's
**Range to 50%**:

```
!`Axis 5-`
```

Two things are needed:

- **Inversion** (the expression above). The GameCube analog triggers rest at the
  *floor* of their HID axis (Dolphin reads a resting trigger as full `Axis N-`),
  so the binding is inverted to read rest = 0 and press = full.
- **Range 50%.** The inverted half-axis treats the axis *midpoint* as "full," so
  every value comes through roughly doubled (LS 49 -> ~100, MS 94 -> ~190). Melee
  treats analog >= 140 as a hard shield, so an un-halved MS reads as a full
  shield instead of medium. Setting the R-Analog input's Range to 50% scales it
  back: LS ~= 49 (light), MS ~= 94 (medium). You can confirm the values with
  Dolphin's TAS Input window (the L/R sliders read 0-255).

Hard shield is unaffected either way -- it comes from the digital **R**
(`Button 5`), not the analog value. L is digital in Melee, so bind **L** to its
digital button and leave **L-Analog** empty.

### Enable "Background Input"

In the controller config **Options**, check **Background Input**.

Without it, when the game window loses focus Dolphin gates every input to 0
before evaluating the mappings. Because R-Analog is the one inverted binding,
`!0` evaluates to a *full* R press — so R looks stuck whenever you click away
from the game. Enabling Background Input keeps Dolphin reading the real axis, so
the inversion never sees a zeroed input.

This is a property of HayBox's USB trigger output (identical to the RP2040/pico
backends), not specific to the Frame1 — any HayBox board driven over USB into
Dolphin needs the same two settings.
