#include "pinfinder.hpp"

#ifdef FRAME1_PINFINDER

#include "comms/N64Console.hpp"
#include "comms/n64_report.hpp"

#include <Adafruit_TinyUSB.h>
#include <Arduino.h> // STM32duino: also pulls in the STM32 HAL (flash)

/*
 * Frame1 Joybus pin brute-forcer.  [diagnostic build only]
 *
 * Build:  pio run -e frame1_pinfinder
 * Flash:  exactly like the normal firmware (hold Up, plug in, DFU).
 *
 * The Frame1's USB-C-pad -> STM32-GPIO mapping for the console data line is
 * undocumented, and the normal scan only probes "free" pins - it deliberately
 * skips the USB (PA11/PA12) and SWD (PA13/PA14) pins. But off a USB host and a
 * debugger (i.e. when powered through the Retro-C cable + N64 adapter) those
 * pins are fair game, and the data line could well be routed onto one of them.
 * So this tries them too.
 *
 * It picks a mode from whether a USB host is present:
 *
 *   HUNT (on the Retro-C + N64 adapter, no USB host):
 *     For every candidate GPIO, briefly act as an N64 controller - answer the
 *     adapter's identify and watch for a real poll (the active DetectActive
 *     handshake). The first pin the adapter actually polls is THE data pin:
 *       - lock (poll seen)      -> save it + run as a live N64 controller
 *       - traffic but no lock   -> still the data pin (RX works, TX/timing off);
 *                                  save it so we at least learn the pin
 *     The result (pin + status) is written to a spare flash page so it can be
 *     read back in the other mode.
 *
 *   REPORT (on a plain USB-C -> USB-A cable, USB enumerates):
 *     Read the saved result from flash and print it over USB serial. This is
 *     how you learn the pin without a logic analyser: hunt on the adapter, then
 *     replug the plain cable and `pio device monitor -e frame1_pinfinder`.
 *
 * Reflash the normal `frame1` build when done (holding Up at power-on always
 * re-enters DFU, so this cannot brick the board).
 */

// ---------------------------------------------------------------------------
// Spare flash page used to carry the hunt result across to the report session.
// 0x08040000 is in the large gap between the app (0x08020000, ~100 KB) and the
// config region (0x0807C000); DFU only erases the app's own pages and config
// saves only touch 0x0807C000+, so this page is never otherwise written.
// ---------------------------------------------------------------------------
namespace {

constexpr uint32_t kScratchAddr = 0x08040000UL;
constexpr uint32_t kScratchMagic = 0xF1A5B002UL;

enum : uint8_t {
    kStatusNotFound = 0,
    kStatusLocked = 1,
    kStatusTraffic = 2,
};

struct Candidate {
    uint8_t pin;
    const char *name;
};

// Everything the normal scan probes, PLUS PA11/PA12/PA13/PA14. Button-matrix
// pins (which idle LOW through their pull-downs) and the HSE crystal (PF0/PF1)
// are left out; the idle-high pre-check skips anything else that idles low.
const Candidate kCandidates[] = {
    { PA0, "PA0" },   { PA1, "PA1" },   { PA2, "PA2" },   { PA3, "PA3" },
    { PA4, "PA4" },   { PA5, "PA5" },   { PA6, "PA6" },   { PA7, "PA7" },
    { PA8, "PA8" },   { PA9, "PA9" },   { PA10, "PA10" }, { PA11, "PA11" },
    { PA12, "PA12" }, { PA13, "PA13" }, { PA14, "PA14" }, { PA15, "PA15" },
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
constexpr size_t kCandidateCount = sizeof(kCandidates) / sizeof(kCandidates[0]);

constexpr uint32_t kHuntWindowMs = 120;  // max active-probe window per pin

void scratch_write(uint8_t idx, uint8_t status, uint8_t rawpin, uint8_t released, uint8_t driven) {
    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_WRPERR | FLASH_FLAG_PGERR);

    FLASH_EraseInitTypeDef erase = {};
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = kScratchAddr;
    erase.NbPages = 1;
    uint32_t page_error = 0;
    if (HAL_FLASHEx_Erase(&erase, &page_error) == HAL_OK) {
        uint32_t w1 = (uint32_t)idx | ((uint32_t)status << 8) | ((uint32_t)rawpin << 16);
        uint32_t w2 = (uint32_t)released | ((uint32_t)driven << 8);
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, kScratchAddr, kScratchMagic);
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, kScratchAddr + 4, w1);
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, kScratchAddr + 8, w2);
    }
    HAL_FLASH_Lock();
}

bool scratch_read(uint8_t *idx, uint8_t *status, uint8_t *rawpin, uint8_t *released,
                  uint8_t *driven) {
    const volatile uint32_t *p = reinterpret_cast<const volatile uint32_t *>(kScratchAddr);
    if (p[0] != kScratchMagic) {
        return false;
    }
    uint32_t w1 = p[1];
    uint32_t w2 = p[2];
    *idx = (uint8_t)(w1 & 0xFF);
    *status = (uint8_t)((w1 >> 8) & 0xFF);
    *rawpin = (uint8_t)((w1 >> 16) & 0xFF);
    *released = (uint8_t)(w2 & 0xFF);
    *driven = (uint8_t)((w2 >> 8) & 0xFF);
    return true;
}

// Can we actually pull `pin` LOW through our open-drain driver (the exact
// digitalWriteFast path JoybusIO uses)? On the adapter the line has a pull-up,
// so when released it should read HIGH, and when we hold it low it should read
// LOW if our drive works. Returns the HIGH-sample counts (out of 100) for each.
void drive_test(uint8_t pin, uint8_t *released_high, uint8_t *driven_high) {
    PinName pn = digitalPinToPinName(pin);
    pinMode(pin, OUTPUT_OPEN_DRAIN);

    digitalWriteFast(pn, HIGH);  // release
    delayMicroseconds(50);
    int rh = 0;
    for (int i = 0; i < 100; i++) {
        if (digitalReadFast(pn) == HIGH) rh++;
        delayMicroseconds(10);
    }

    digitalWriteFast(pn, LOW);  // hold low
    delayMicroseconds(50);
    int dh = 0;
    for (int i = 0; i < 100; i++) {
        if (digitalReadFast(pn) == HIGH) dh++;
        delayMicroseconds(10);
    }

    digitalWriteFast(pn, HIGH);  // release
    *released_high = (uint8_t)rh;
    *driven_high = (uint8_t)dh;
}

// A plausible idle Joybus line sits HIGH between commands (the host pulls it up).
// Skip anything that idles low (button pulled-down pins, SWCLK's pull-down, etc.)
// so they can't be mistaken for bus traffic.
bool pin_idles_high(uint8_t pin) {
    pinMode(pin, INPUT_PULLUP);
    delayMicroseconds(300);
    int high = 0;
    for (int i = 0; i < 24; i++) {
        if (digitalRead(pin) == HIGH) {
            high++;
        }
        delayMicroseconds(100);
    }
    return high >= 20;
}

// After a lock: act as a real N64 controller on `pin`. Reads a handful of the
// Frame1's buttons directly (active-high optical sensors: pressed = HIGH) and
// adds a slow auto stick sweep so evtest moves even with no input pressed.
void run_n64_controller(uint8_t pin) {
    N64Console n64(pin);
    n64.Init();

    pinMode(PB1, INPUT);   // A
    pinMode(PE15, INPUT);  // B
    pinMode(PE11, INPUT);  // Z
    pinMode(PD2, INPUT);   // Start
    pinMode(PE2, INPUT);   // Right
    pinMode(PE3, INPUT);   // Down
    pinMode(PE4, INPUT);   // Left
    pinMode(PE9, INPUT);   // Up

    uint32_t t0 = millis();
    for (;;) {
        n64_report_t rep = default_n64_report;
        rep.a = (digitalRead(PB1) == HIGH);
        rep.b = (digitalRead(PE15) == HIGH);
        rep.z = (digitalRead(PE11) == HIGH);
        rep.start = (digitalRead(PD2) == HIGH);
        rep.dpad_right = (digitalRead(PE2) == HIGH);
        rep.dpad_down = (digitalRead(PE3) == HIGH);
        rep.dpad_left = (digitalRead(PE4) == HIGH);
        rep.dpad_up = (digitalRead(PE9) == HIGH);

        int phase = (int)(((millis() - t0) / 500) % 4);
        rep.stick_x = (phase == 1) ? 60 : (phase == 3) ? -60 : 0;

        if (n64.WaitForPoll() == N64PollStatus::OK) {
            n64.SendReport(&rep);
        }
    }
}

void hunt_mode() {
    int locked_idx = -1;
    int traffic_idx = -1;

    for (size_t i = 0; i < kCandidateCount; i++) {
        uint8_t pin = kCandidates[i].pin;
        if (!pin_idles_high(pin)) {
            continue;  // idles low -> not a Joybus line
        }
        int r = N64Console(pin).DetectActive(kHuntWindowMs);
        if (r == 1) {
            locked_idx = (int)i;
            break;
        }
        if (r == 0 && traffic_idx < 0) {
            traffic_idx = (int)i;  // data pin found, but handshake didn't complete
        }
    }

    if (locked_idx >= 0) {
        scratch_write((uint8_t)locked_idx, kStatusLocked, kCandidates[locked_idx].pin, 0, 0);
        run_n64_controller(kCandidates[locked_idx].pin);  // never returns
    } else if (traffic_idx >= 0) {
        // Found the data pin but the handshake didn't complete. Test whether we
        // can even pull it low (open-drain) to separate a drive bug from timing.
        uint8_t released = 0, driven = 0;
        drive_test(kCandidates[traffic_idx].pin, &released, &driven);
        scratch_write(
            (uint8_t)traffic_idx, kStatusTraffic, kCandidates[traffic_idx].pin, released, driven
        );
    } else {
        scratch_write(0, kStatusNotFound, 0, 0, 0);
    }

    for (;;) { /* idle; replug the plain USB cable to read the result */ }
}

void report_mode() {
    uint8_t idx = 0, status = 0, rawpin = 0, released = 0, driven = 0;
    bool have = scratch_read(&idx, &status, &rawpin, &released, &driven);
    const char *name = (have && idx < kCandidateCount) ? kCandidates[idx].name : "?";

    for (;;) {
        TinyUSB_Device_Task();
        Serial.println();
        Serial.println("=== Frame1 Joybus pin brute-forcer: result ===");
        if (!have) {
            Serial.println("No hunt result stored yet.");
            Serial.println("Flash this build, plug into the N64 adapter, wait ~10s, then replug here.");
        } else if (status == kStatusLocked) {
            Serial.print("LOCKED  -> data pin = ");
            Serial.println(name);
            Serial.println("Handshake completed. Set pinout.joybus_data to this pin and the");
            Serial.println("normal `frame1` build will work on N64.");
        } else if (status == kStatusTraffic) {
            Serial.print("TRAFFIC (no lock) -> data pin = ");
            Serial.println(name);
            Serial.print("Drive test: released reads HIGH ");
            Serial.print(released);
            Serial.print("/100, driven-low reads HIGH ");
            Serial.print(driven);
            Serial.println("/100.");
            if (released > 50 && driven < 20) {
                Serial.println("  -> Open-drain drive WORKS (we can pull the line low).");
                Serial.println("     The fault is bit timing; a logic analyzer is the next step.");
            } else if (driven >= 20) {
                Serial.println("  -> We CANNOT pull the line low: open-drain drive is NOT working.");
                Serial.println("     This is a firmware GPIO bug - tell Claude this result.");
            } else {
                Serial.println("  -> Inconclusive (no clear pull-up seen). Re-run on the adapter.");
            }
        } else {
            Serial.println("NO Joybus traffic on any candidate pin.");
            Serial.println("Either the data line isn't among the probed pins, or RX/GPIO is broken.");
        }
        Serial.flush();
        TinyUSB_Device_FlushCDC();
        delay(1500);
    }
}

} // namespace

void frame1_pinfinder() {
    // setup() already ran TinyUSB_Device_Init(). Re-present so a real host can
    // enumerate the CDC port (for report mode); harmless on the adapter.
    Serial.begin(115200);
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();

    // A USB host (plain cable) mounts us within ~1 s; the N64 adapter never does.
    bool mounted = false;
    uint32_t t0 = millis();
    while (millis() - t0 < 2500) {
        TinyUSB_Device_Task();
        TinyUSB_Device_FlushCDC();
        if (TinyUSBDevice.mounted()) {
            mounted = true;
            break;
        }
        delay(5);
    }

    if (mounted) {
        report_mode();  // never returns
    } else {
        hunt_mode();    // never returns
    }
}

#endif  // FRAME1_PINFINDER
