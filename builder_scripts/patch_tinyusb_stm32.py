"""Install the Frame1 STM32F303 (fsdev) TinyUSB port.

Adafruit_TinyUSB_Arduino's stock STM32 port
(src/arduino/ports/stm32/Adafruit_TinyUSB_stm32.cpp) only supports the USB-OTG
peripheral (F4/F7/H7) and does not compile for the F303's USB FS device
controller. Since the library lives under .pio/libdeps (outside the repo and
not editable in place), this pre-build script copies our F303 replacement
(HAL/stm32/tinyusb/Adafruit_TinyUSB_stm32.cpp) over it before the library is
compiled.

Runs every build; a no-op once the correct port is already in place.
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
