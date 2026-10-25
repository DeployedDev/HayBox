# Frame1 Heavy and Light (STM32F303VE)

HayBox port for the Frame1 leverless controller. One firmware build runs on both
the Heavy and the Light, as with Frame1's own firmware. It aims for parity with
the reference pico (RP2040) build: the same USB backends and plug-in button
holds, the same console backends and console detection, and the same game modes,
so nothing needs relearning coming from any other HayBox board.

## Heavy vs Light

The two models share the button-to-GPIO map but use opposite switch polarity:
the Heavy's optical switches drive a pin HIGH when pressed, the Light's
mechanical switches pull it LOW. As in the stock firmware, the board tells them
apart at boot from two strap inputs (PB12/PB13): both low means a Heavy, either
high means a Light. Buttons use the pico's 5 ms debounce on both models.

The Heavy has been tested on hardware. The Light follows the stock firmware's
logic but has not been tested yet; reports are welcome.

## USB modes (hold while plugging in)

| Hold at plug-in | Mode | Enumerates as |
|---|---|---|
| *nothing* | **XInput** (default) | `0738:4726` Xbox 360 Controller |
| **Z** | DInput HID gamepad | `239a:cafe Adafruit FRAME1` |
| **X** | Nintendo Switch | `0f0d:0092` HORI POKKEN CONTROLLER |
| **Start** | Configurator (serial) | `239a:cafe` CDC serial port |

**Up** is reserved: it enters the DFU bootloader (a hardware check that runs
before the firmware). Game-mode profiles and their combos (Mod X + Start + L /
Left / Down / Right / B / R, Mod Y + Start + L) are identical to stock HayBox.

## Console support (wired)

Plugged into a console instead of a PC, the board detects the console the same
way the pico does (no USB host after 500 ms, then a GameCube probe, then N64)
and runs that backend. No wiring is needed: the GameCube/N64 data line is
carried on the Frame1's USB-C port (pin `PD3`), so a USB-C to GameCube or N64
cable (Retro-C style) plugs straight in.

| Console | Status |
|---|---|
| **N64** | Working. Validated through a Raphnet N64-to-USB adapter (HID mode, and the raphnetraw emulator plugin with Super Mario 64 and Ocarina of Time); not yet on a real console. |
| **GameCube / Wii** | Ported, untested. Needs a GameCube console or GameCube adapter to validate. |
| **NES / SNES** | Not wired, as on the reference pico board (`nes_*` pins are `-1`). |

The STM32F303 has no PIO, so the Joybus protocol is a cycle-timed bit-bang
implementation of what the pico does with PIO. Like the pico, it answers the
console's identify and poll commands and leaves controller-pak commands
unanswered (no pak emulated).

> **Testing N64 through a GameCube/N64 adapter:** adapters that speak both
> protocols (such as the Raphnet) answer the GameCube probe, so the board starts
> in GameCube mode on them. A real N64 never does, so this only affects testing.
> To test N64 on such an adapter, temporarily skip the GameCube probe on your
> machine (don't commit this):
>
> ```sh
> sed -i 's/} else if (GamecubeConsole(pinout.joybus_data)/} else if (false \&\& GamecubeConsole(pinout.joybus_data)/' HAL/stm32/src/comms/console_detection.cpp
> ```
>
> Undo it afterwards with
> `git checkout -- HAL/stm32/src/comms/console_detection.cpp`.

## Configurator and saved config

Hold **Start** while plugging in to enter the Configurator, then connect with
the HayBox web configurator over the serial port it presents.

**Linux: serial port permissions.** Your user must be able to open the port
(`/dev/ttyACM0`), or the browser logs `Failed to open serial port:
FILE_ERROR_ACCESS_DENIED` in `chrome://device-log` and the configurator fails
to get device info. On Arch the device belongs to the `uucp` group (`dialout`
on Debian/Ubuntu):

```sh
sudo usermod -aG uucp "$USER"   # then log out and back in
```

or, to avoid re-logging in, a udev rule for this device (takes effect on
re-plug):

```sh
sudo tee /etc/udev/rules.d/51-haybox.rules >/dev/null <<'EOF'
SUBSYSTEM=="tty", ATTRS{idVendor}=="239a", ATTRS{idProduct}=="cafe", MODE="0666"
EOF
sudo udevadm control --reload-rules && sudo udevadm trigger
```

Saved settings
are stored in a 16 KB region reserved at the top of flash
(`0x0807C000`-`0x0807FFFF`), which is above anything `dfu-util` writes when
flashing `firmware.bin`, so **your config survives firmware updates**. On first
boot (or if the stored config fails its CRC check) the firmware writes the
defaults there.

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

### Flashing a prebuilt image (Windows, or without PlatformIO)

Every build produces two images in `.pio/build/frame1/`:

- **`firmware.hex`** - Intel HEX. It **embeds the `0x08020000` load address**,
  so GUI flashers place it correctly with nothing to type. Use this one.
- **`firmware.bin`** - raw image for `dfu-util`, which needs the address given
  explicitly (`-s 0x08020000`).

The board uses standard STM32 DFU, so any DFU tool works. Options, easiest
first (pick **one** DFU-driver-based tool per PC - they install different
drivers for the DFU device and conflict):

- **Frame1 firmware updater** (the vendor's tool). Hold **Up**, plug in, and
  load `firmware.hex`. Confirm on one board first that it accepts a
  third-party image; if it does, this is the simplest path for everyone.
- **STM32CubeProgrammer** (ST's official GUI). Install it, hold **Up** and plug
  in, connection type **USB** -> Refresh -> Connect. In *Erasing &
  Programming*, browse to `firmware.hex` (the address is read from the file;
  with `firmware.bin` you must set Start address `0x08020000`), tick Verify,
  Start Programming. Re-plug when done.
- **dfu-util**. With the board in DFU, run Zadig once and install **WinUSB**
  on "STM32 BOOTLOADER", then: `dfu-util -a 0 -s 0x08020000 -D firmware.bin`.

On Windows a bare plug-in (XInput) is recognised as an Xbox 360 controller and
works in Dolphin through its native XInput backend with no extra mapping.

## Using with Dolphin (DInput mode - hold Z at plug-in)

In DInput mode the board enumerates as **`Adafruit FRAME1`**. Select it as the
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

## Diagnostic build

`pio run -e frame1_pinfinder` builds the tool that was used to find the
console data pin. It is not needed for normal use; flash the `frame1` build for
that.
