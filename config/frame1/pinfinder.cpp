#include "pinfinder.hpp"

#ifdef FRAME1_PINFINDER

#include <Adafruit_TinyUSB.h>
#include <Arduino.h>

/*
 * Frame1 GameCube data-pin finder.  [diagnostic build only]
 *
 * Build:  pio run -e frame1_pinfinder
 * Flash:  exactly like the normal firmware (hold Up, plug in, DFU).
 *
 * It sets every FREE GPIO (everything not used by the button matrix, USB, SWD,
 * or the HSE crystal) to INPUT_PULLUP so they idle HIGH, then streams pin
 * transitions over the USB serial port.
 *
 * To use: flash it, open a serial monitor at 115200, then momentarily touch the
 * GameCube cable's DATA wire to GND. The one pin that prints "-> LOW" is the
 * value to put in pinout.joybus_data (config/frame1/config.cpp). No console and
 * no multimeter needed. Reflash the normal `frame1` build when done (holding Up
 * at power-on always re-enters DFU, so this cannot brick the board).
 *
 * The scanned set is the pins NOT in the button map; if the variant doesn't
 * define one of these macros the build will name it - just delete that row.
 */

struct DiagPin {
    uint32_t pin;
    const char *name;
};

static const DiagPin kPins[] = {
    { PA0, "PA0" },   { PA1, "PA1" },   { PA2, "PA2" },   { PA3, "PA3" },
    { PA4, "PA4" },   { PA5, "PA5" },   { PA6, "PA6" },   { PA7, "PA7" },
    { PA8, "PA8" },   { PA9, "PA9" },   { PA10, "PA10" }, { PA15, "PA15" },
    { PB3, "PB3" },   { PB4, "PB4" },   { PB5, "PB5" },   { PB6, "PB6" },
    { PB7, "PB7" },   { PB8, "PB8" },   { PB9, "PB9" },   { PB11, "PB11" },
    { PB12, "PB12" }, { PB13, "PB13" }, { PB14, "PB14" }, { PB15, "PB15" },
    { PC0, "PC0" },   { PC1, "PC1" },   { PC2, "PC2" },   { PC3, "PC3" },
    { PC6, "PC6" },   { PC7, "PC7" },   { PC8, "PC8" },   { PC9, "PC9" },
    { PC10, "PC10" }, { PC11, "PC11" }, { PC12, "PC12" }, { PC13, "PC13" },
    { PC14, "PC14" }, { PC15, "PC15" },
    { PD0, "PD0" },   { PD1, "PD1" },   { PD3, "PD3" },   { PD4, "PD4" },
    { PD5, "PD5" },   { PD6, "PD6" },   { PD7, "PD7" },   { PD8, "PD8" },
    { PD9, "PD9" },   { PD10, "PD10" }, { PD11, "PD11" }, { PD12, "PD12" },
    { PD13, "PD13" }, { PD14, "PD14" }, { PD15, "PD15" },
    { PE0, "PE0" },   { PE1, "PE1" },   { PE6, "PE6" },
};
static const size_t kPinCount = sizeof(kPins) / sizeof(kPins[0]);

void frame1_pinfinder() {
    // The USB device stack was already started by setup() (TinyUSB_Device_Init).
    Serial.begin(115200);
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();

    // Pump the stack a while so the host can enumerate and open the CDC port.
    for (uint32_t i = 0; i < 400; i++) {
        TinyUSB_Device_Task();
        TinyUSB_Device_FlushCDC();
        delay(5);
    }

    static bool prev_high[kPinCount];
    for (size_t i = 0; i < kPinCount; i++) {
        pinMode(kPins[i].pin, INPUT_PULLUP);
    }
    delay(5);
    for (size_t i = 0; i < kPinCount; i++) {
        prev_high[i] = (digitalRead(kPins[i].pin) == HIGH);
    }

    Serial.println();
    Serial.println("=== Frame1 GameCube data-pin finder ===");
    Serial.println("Free pins set to INPUT_PULLUP (idle HIGH).");
    Serial.println("Touch the GameCube cable DATA wire to GND.");
    Serial.println("The pin that prints '-> LOW' is your pinout.joybus_data.");
    Serial.print("(pins already LOW at startup, ignore these): ");
    for (size_t i = 0; i < kPinCount; i++) {
        if (!prev_high[i]) {
            Serial.print(kPins[i].name);
            Serial.print(' ');
        }
    }
    Serial.println();
    Serial.println("Watching...");
    Serial.flush();

    for (;;) {
        TinyUSB_Device_Task();
        for (size_t i = 0; i < kPinCount; i++) {
            bool now_high = (digitalRead(kPins[i].pin) == HIGH);
            if (now_high != prev_high[i]) {
                Serial.print(kPins[i].name);
                Serial.println(now_high ? " -> HIGH" : " -> LOW   <== joybus_data candidate");
                prev_high[i] = now_high;
            }
        }
        TinyUSB_Device_FlushCDC();
        delay(2);
    }
}

#endif  // FRAME1_PINFINDER
