"""Emit firmware.hex alongside firmware.bin for the Frame1 (post-build).

PlatformIO produces firmware.bin for dfu-util. An Intel HEX image is more
convenient for GUI flashers: it embeds the load address (0x08020000, the app
region above the Frame1's protected DFU bootloader), so tools such as the
Frame1 firmware updater or STM32CubeProgrammer place it correctly without the
user typing an address.

Runs as a post action on the ELF, so it is a no-op when nothing was rebuilt.
"""

Import("env")  # noqa: F821

env.AddPostAction(  # noqa: F821
    "$BUILD_DIR/${PROGNAME}.elf",
    env.VerboseAction(  # noqa: F821
        " ".join(
            [
                "$OBJCOPY",
                "-O",
                "ihex",
                "$BUILD_DIR/${PROGNAME}.elf",
                "$BUILD_DIR/${PROGNAME}.hex",
            ]
        ),
        "Building $BUILD_DIR/${PROGNAME}.hex",
    ),
)
