# Frame1 Heavy and Light (STM32F303VE) - ruleset-compliant build

Port of the **ruleset-compliant HayBox** (CarVac's tree, the firmware line
approved under the GENESIS / Melee controller ruleset) for the Frame1 Heavy and
Light leverless controllers (one firmware build for both, as with Frame1's own). The game modes, SOCD handling and the Melee output
limiter are the shared `src/` code of that tree, used unchanged; this port only
adds the STM32 hardware layer and the board definition. The USB and console
backends and the plug-in holds are the same as that tree's RP2040 boards, so
nothing needs relearning coming from one of them. The hardware layer is shared
with the upstream-HayBox Frame1 port (branch `frame1-stm32-port`), where it was
validated on hardware.

## What "ruleset-compliant" means here

In **Melee mode** the shared code enforces the controller ruleset regardless of
the stored config:

- **Neutral SOCD** on both axes (the configured SOCD type is overridden to
  neutral for Melee).
- **Travel time** and the ruleset's **input-rate limits** (SDI / pivot / tap
  limits with coordinate fuzzing), applied by the output limiter
  (`src/modes/MeleeLimits.cpp`).

On the RP2040 the limiter runs on the second core at exactly 2 kHz. The F303
has a single core, so the same scan / game mode / limiter sequence runs on a
fixed 500 us schedule (`HAL/stm32/src/comms/MeleeTicker.cpp`, driven from
`config/frame1/config.cpp`). The USB backends never block in Melee mode, so the
main loop keeps that schedule. In GameCube mode the backend runs the ticks
itself while it waits for the console's poll, keeping them clear of the time
the next poll is expected (learned from the console's own polling); a tick
held back for a poll runs right after the reply. Other game modes are
unaffected by the limiter, as on the RP2040 boards.

> **GameCube in Melee mode on this build is new and not yet tested on
> hardware.** If inputs drop or the controller disconnects in Melee on a
> GameCube adapter or console, please report it.

This build is based on the **Legacy** release line (`Build23.1_RP2040_Legacy`,
the Melee coordinate set the stock mode has always used). CarVac's tree also
publishes a **Wider** line (`Build23.1_RP2040_Wider`); the two differ only in
`src/modes/Melee20Button.cpp`, so a Wider Frame1 build is that one file swapped.

## Heavy vs Light

The two models share the button-to-GPIO map but use opposite switch polarity:
the Heavy's optical switches drive a pin HIGH when pressed, the Light's
mechanical switches pull it LOW. As in the stock firmware, the board tells them
apart at boot from two strap inputs (PB12/PB13): both low means a Heavy, either
high means a Light. Buttons use the pico's 5 ms debounce on both models.

Both models have been tested on hardware with one firmware build of the
upstream-HayBox Frame1 port, which uses the same strap logic: USB mode (evtest
and Dolphin) and N64 (through a Raphnet N64-to-USB adapter).

## USB modes (hold while plugging in)

| Hold at plug-in | Mode | Enumerates as |
|---|---|---|
| *nothing* | **XInput** (default) | `0738:4726` Xbox 360 Controller |
| **Z** | DInput HID gamepad | `239a:cafe Adafruit FRAME1` |
| **X** | Nintendo Switch | `0f0d:0092` HORI POKKEN CONTROLLER |
| **C-Down** | Configurator (serial) | `239a:cafe` CDC serial port |

(The Configurator hold is **C-Down** in this tree, matching its RP2040 boards;
upstream HayBox uses Start.) **Up** is reserved: it enters the DFU bootloader
(a hardware check that runs before the firmware). Game-mode profiles and their
combos (Mod X + Start + L / Left / Down / Right / B, Mod Y + Start + L) are
identical to the tree's RP2040 defaults.

Nintendo Switch mode connects directly over USB (dock or USB-C) as a wired
controller, starting in the Ultimate profile.

## Console support (wired)

Plugged into a console instead of a PC, the board detects the console the same
way the pico does (no USB host after 500 ms, then a GameCube probe, then N64)
and runs that backend. (The pico also counts NES/SNES clock pulses first; the
Frame1 has no NES/SNES lines and this tree's pinout cannot mark them unwired,
so that step is skipped.) No wiring is needed: the Frame1's USB-C port follows the
[Retro-C](https://docs.handheldlegend.com/s/portal/doc/gamecube-retro-c-cable-information-PdlHLu5LBR)
pinout, so a Retro-C-compatible USB-C to GameCube or N64 cable plugs straight in,
either way up:

| USB-C pins | Retro-C role | On the Frame1 |
|---|---|---|
| A11, B11 | Console data | Joybus data line, `PD3` |
| A2, B2 | Console 3.3 V | Connected |
| VBUS / GND | 5 V / ground | Normal USB power and ground |

This was checked with a multimeter through a breakout board: those four pins
are the only ones connected besides USB 2.0 (D+/D-, CC); SBU and the other
high-speed pins are unused.

Status below is from the upstream-HayBox Frame1 port, which shares this
hardware layer; GameCube in Melee mode additionally uses the limiter
scheduling described above.

| Console | Status |
|---|---|
| **N64** | Working. Validated through a Raphnet N64-to-USB adapter (HID mode, and the raphnetraw emulator plugin with Super Mario 64 and Ocarina of Time); not yet on a real console. |
| **GameCube / Wii** | Working. Validated through a Wii U style GameCube adapter (Input Integrity Lossless adapter) in Dolphin, with exact stick and trigger values; not yet on a real GameCube/Wii console. |
| **NES / SNES** | Not wired. NES/SNES need three data lines and the USB-C port has no spare ones, so this needs a hardware change. |

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

Hold **C-Down** while plugging in to enter configurator mode. HayBox has no
official hosted configurator; use a HayBox-compatible one such as
[remapp.ing](https://remapp.ing/) (tested with the Frame1: remapping and saving
both work). It uses Web Serial, so open it in a Chromium-based browser (Chrome,
Chromium, Brave, Edge), click Connect and pick the Frame1's serial port.

remapp.ing does not auto-detect the Frame1 (the firmware reports its device
name as `frame1`), so choose the **Open Frame1** layout when asked. It has the
same buttons in the same positions with the same HayBox button assignments, so
it is an exact match, not an approximation.

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

**Updating from an earlier build of this branch:** settings saved by the
earlier USB-only build list only the USB backends. On a console the board
still detects GameCube/N64, but starts with no game mode selected, so nothing
responds until you pick one with its combo (e.g. Mod X + Start + L for Melee),
which is needed again after each plug-in until the saved settings are reset to
the new defaults.

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

- **Frame1 firmware updater** (the vendor's tool). Confirmed to work. The
  updater only accepts files whose name starts with **`Frame1`** (capital F),
  so first rename `firmware.hex`, e.g. to `Frame1_HayBox.hex`. Then hold
  **Up**, plug in, and load the renamed file. This is the simplest path for
  most people.
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

In DInput mode the board enumerates as **`Adafruit FRAME1`**. In Dolphin (Slippi
or the Project+ build; each has its own controller settings), set Port 1 to
**Standard Controller**, select the device, and map it as below. This matches
HayBox's own profile, `dolphin/HayBox_DInput_Linux.ini`; only the device name
differs. Tested on Linux with Slippi and Project+.

| Dolphin input | Mapping | Dolphin input | Mapping |
|---|---|---|---|
| A | `Button 1` | Main Stick Up / Down | `Axis 1-` / `Axis 1+` |
| B | `Button 0` | Main Stick Left / Right | `Axis 0-` / `Axis 0+` |
| X | `Button 3` | C-Stick Up / Down | `Axis 4-` / `Axis 4+` |
| Y | `Button 2` | C-Stick Left / Right | `Axis 3-` / `Axis 3+` |
| Z | `Button 4` | D-Pad Up / Down | `Axis 7-` / `Axis 7+` |
| Start | `Button 9` | D-Pad Left / Right | `Axis 6-` / `Axis 6+` |
| L (digital) | `Button 7` | L-Analog | `` `Axis 2-+` `` |
| R (digital) | `Button 5` | R-Analog | `` `Axis 5-+` `` |

Newer Dolphin builds name the buttons instead (`SOUTH` = B, `EAST` = A, `C` =
Y, `NORTH` = X, `WEST` = Z, `Z` = R, `TR` = L, `TR2` = Start). Clicking a box
and pressing the Frame1 button fills it in either way.

Settings that are not obvious:

- **Analog triggers: type `` `Axis 2-+` `` and `` `Axis 5-+` `` (with the
  backticks), Range 100%, Trigger Threshold 0.** `-+` makes Dolphin read the
  trigger's full range, from rest (0) to full. Check in **Tools -> TAS Input**:
  the R slider reads 0 at rest, **49** for LS and **95** for MS (Melee mode);
  R reads 255 because the digital R press makes Dolphin report a full trigger.
  Hard shield and airdodge come from the digital L/R.
- **Stick Dead Zone 0.**
- **Background Input on** (in the Controllers window's options). Otherwise
  Dolphin ignores the controller whenever its game window is not focused,
  including while the TAS Input window is.

### Stick values over USB are scaled by Dolphin

HayBox's stick values are raw GameCube values, and over a GameCube connection
Dolphin passes them through unchanged. Over USB (DInput or XInput), Dolphin
converts the gamepad's axes into its emulated stick and the values come out
**shorter**: angles are preserved, but distances are scaled down (most on
diagonals). For example, Mod X + Up-Right + C-Right in Project+ mode sends
(72, 61) and reads about (42, 36) in TAS Input. This applies to any HayBox
controller over USB, not just the Frame1. For exact values in Dolphin, connect
through a Wii U style GameCube adapter (port set to "GameCube Adapter for Wii
U") with a USB-C to GameCube cable, so the board runs its GameCube mode.
Tested this way, values arrive exactly as sent (e.g. Mod X + Right reads 181,
LS 49, MS 94 in TAS Input).
