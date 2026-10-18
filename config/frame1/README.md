# Frame1 Heavy (STM32F303VE)

USB port of HayBox for the Frame1 Heavy leverless controller. At power-on it
picks one of two USB identities: by default it emulates Nintendo's **GameCube
Adapter for Wii U**, which is the mode to use for Melee; hold **Start** while
plugging in for a plain DInput HID gamepad instead.

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

## GameCube Adapter for Wii U mode (default - use this for Melee)

With nothing held at plug-in, the Frame1 enumerates as Nintendo's WUP-028
adapter (`057e:0337`). In Dolphin set **Controllers -> Port 1 -> GameCube
Adapter for Wii U**. Dolphin then reads the controller **natively**: exact
0-255 GameCube values straight from the firmware (light/medium shield 49/94,
full cardinal 208, modifier angles to the digit) with **no** input mapping,
Range, inversion or Background Input needed. This is also what Slippi netplay
expects.

### Linux: udev rule (required)

Dolphin opens the adapter directly through libusb, so your user needs
permission to the device. Without this Dolphin reports **"No adapter detected"**
even though the firmware is fine.

```sh
sudo tee /etc/udev/rules.d/51-gcadapter.rules >/dev/null <<'EOF'
SUBSYSTEM=="usb", ENV{DEVTYPE}=="usb_device", ATTRS{idVendor}=="057e", ATTRS{idProduct}=="0337", MODE="0666"
EOF
sudo udevadm control --reload-rules && sudo udevadm trigger
```

Then re-plug the controller.

### Checking it works

1. `lsusb` lists a `057e:0337 Nintendo Co., Ltd` entry (if it still shows
   `239a:cafe`, the board came up in DInput mode - re-plug without holding
   anything).
2. Dolphin -> Controllers -> Port 1 -> **GameCube Adapter for Wii U** ->
   Configure shows **"Adapter detected"**.
3. The controller moves in-game with no further setup.

## Standard Controller (DInput) mode - hold Start at plug-in

In this mode the board enumerates as **`Adafruit FRAME1`**. Select it as the
device, set it to **Standard Controller**, and map the buttons as normal. Two
settings are not obvious:

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
