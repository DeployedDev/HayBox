"""Pre-build patches for the Frame1 (STM32F303) TinyUSB stack.

Both patches touch libraries under .pio/libdeps (outside the repo, not editable
in place), so they are applied here before the libraries are compiled. Both
run every build and are no-ops once already applied.

1. Install the F303 (fsdev) TinyUSB port. Adafruit_TinyUSB_Arduino's stock
   STM32 port (src/arduino/ports/stm32/Adafruit_TinyUSB_stm32.cpp) only
   supports the USB-OTG peripheral (F4/F7/H7) and does not compile for the
   F303's USB FS device controller, so our replacement
   (HAL/stm32/tinyusb/Adafruit_TinyUSB_stm32.cpp) is copied over it.

2. Adapt Adafruit_TinyUSB_XInput to TinyUSB >= 0.16. The XInput library is
   pinned to the same commit the pico env uses, but that commit calls
   usbd_edpt_xfer() with four arguments. arduino-pico bundles an older TinyUSB
   where that was the signature; the standalone Adafruit TinyUSB library we
   use on STM32 bundles TinyUSB 0.20, which added a trailing `bool is_isr`.
   All three calls run from task context (tud_task), so `false` is appended.
   It also allocates its own interface and endpoint numbers, which Adafruit
   TinyUSB 3.x requires (see shim 3 below). Keeping the pin identical to the
   pico and shimming here is preferred over forking the library.
"""

Import("env")  # noqa: F821

import os
import shutil

project_dir = env.subst("$PROJECT_DIR")  # noqa: F821
libdeps_dir = env.subst("$PROJECT_LIBDEPS_DIR")  # noqa: F821
pioenv = env.subst("$PIOENV")  # noqa: F821

replacement = os.path.join(
    project_dir, "HAL", "stm32", "tinyusb", "Adafruit_TinyUSB_stm32.cpp"
)
target = os.path.join(
    libdeps_dir,
    pioenv,
    "Adafruit TinyUSB Library",
    "src",
    "arduino",
    "ports",
    "stm32",
    "Adafruit_TinyUSB_stm32.cpp",
)

if not os.path.isfile(replacement):
    print("patch_tinyusb_stm32: replacement not found: %s" % replacement)
elif not os.path.isfile(target):
    # Library not installed yet (e.g. a from-scratch build). It will be present
    # on the next build once PlatformIO has fetched the dependency.
    print("patch_tinyusb_stm32: target not present yet: %s" % target)
else:
    with open(replacement, "rb") as f:
        new_contents = f.read()
    with open(target, "rb") as f:
        cur_contents = f.read()
    if new_contents != cur_contents:
        shutil.copyfile(replacement, target)
        print("patch_tinyusb_stm32: installed F303 (fsdev) TinyUSB port")
    else:
        print("patch_tinyusb_stm32: F303 TinyUSB port already in place")


# ---------------------------------------------------------------------------
# 2. Adafruit_TinyUSB_XInput: 4-arg -> 5-arg usbd_edpt_xfer() (TinyUSB >= 0.16)
# ---------------------------------------------------------------------------

xinput_src = os.path.join(
    libdeps_dir, pioenv, "Adafruit_TinyUSB_XInput", "src", "Adafruit_USBD_XInput.cpp"
)

# The two 4-argument call shapes present in the pinned commit. The first form
# appears twice (receive_xinput_report and xinput_xfer_callback), the second
# once (send_xinput_report). Matching on the final argument plus its closing
# paren keeps the edit anchored to these calls only.
XINPUT_SHIMS = (
    (
        "            _xinput_dev->_xinput_out_buffer,\n"
        "            EPSIZE\n"
        "        );",
        "            _xinput_dev->_xinput_out_buffer,\n"
        "            EPSIZE,\n"
        "            false\n"
        "        );",
    ),
    (
        "            (uint8_t *)report,\n"
        "            sizeof(xinput_report_t)\n"
        "        );",
        "            (uint8_t *)report,\n"
        "            sizeof(xinput_report_t),\n"
        "            false\n"
        "        );",
    ),
    # 3. Endpoint and interface numbers. The pinned commit hardcodes EP 0x81 /
    # 0x01 and relies on the USB core to renumber them, and addInterface() used
    # to bump the interface count. Adafruit TinyUSB 3.x does neither: each
    # interface must call allocInterface()/allocEndpoint() itself, as its CDC
    # does. Without this, XInput and the CDC Serial both claim interface 0 and
    # EP 0x81 (the CDC notification EP, 8 bytes), so every 20-byte report goes
    # out as 8+4+8-byte packets that the host reads as separate reports.
    # XInput is added first (XInputBackend re-adds Serial after it), so it
    # gets interface 0 and EP 0x81/0x01, as on a real pad and on the pico.
    (
        "    // usb core will automatically update endpoint number\n"
        "    const uint8_t desc[] = { TUD_XINPUT_DESCRIPTOR(itfnum, 0, EPOUT, EPIN, EPSIZE, _interval_ms) };\n",
        "    // Frame1 shim: Adafruit TinyUSB 3.x does not renumber, so allocate.\n"
        "    uint8_t ep_in = EPIN;\n"
        "    uint8_t ep_out = EPOUT;\n"
        "    if (buf) {\n"
        "        itfnum = TinyUSBDevice.allocInterface(1);\n"
        "        ep_in = TinyUSBDevice.allocEndpoint(TUSB_DIR_IN);\n"
        "        ep_out = TinyUSBDevice.allocEndpoint(TUSB_DIR_OUT);\n"
        "    }\n"
        "    const uint8_t desc[] = { TUD_XINPUT_DESCRIPTOR(itfnum, 0, ep_out, ep_in, EPSIZE, _interval_ms) };\n",
    ),
)

if not os.path.isfile(xinput_src):
    print("patch_tinyusb_stm32: XInput lib not present yet: %s" % xinput_src)
else:
    with open(xinput_src, "r", encoding="utf-8") as f:
        src = f.read()
    patched = src
    for old, new in XINPUT_SHIMS:
        patched = patched.replace(old, new)
    if patched != src:
        with open(xinput_src, "w", encoding="utf-8") as f:
            f.write(patched)
        print("patch_tinyusb_stm32: shimmed XInput lib for Adafruit TinyUSB 3.x")
    elif all(new in src for _, new in XINPUT_SHIMS):
        print("patch_tinyusb_stm32: XInput shims already in place")
    else:
        # Neither the known 4-arg shapes nor the shimmed forms were found: the
        # library pin has changed and this shim needs revisiting.
        print(
            "patch_tinyusb_stm32: WARNING - XInput source did not match expected "
            "call shapes; shim not applied (library pin changed?)"
        )
