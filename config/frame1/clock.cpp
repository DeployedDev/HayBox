#include <Arduino.h>

/*
 * Clock configuration for the Frame1 (STM32F303VE).
 *
 * STM32duino's CMSIS startup (SystemInit) reverts the clock tree to the HSI
 * reset state before main() runs, so the port must bring the PLL up itself - it
 * cannot simply inherit the bootloader's already-running clock the way the bare
 * libopencm3 proof-of-concept did (libopencm3's startup leaves RCC untouched;
 * CMSIS does not).
 *
 * This overrides the WEAK SystemClock_Config() supplied by the nucleo_f303ze
 * variant, which assumes an HSE *bypass* (an external clock fed from the
 * on-board ST-Link). The Frame1 instead has a real 8 MHz HSE *crystal*, so a
 * bypass config would never start the oscillator and would hang in the HAL
 * error handler. We configure the clock tree the Frame1 actually uses - the
 * same one its bootloader sets up and that we validated at the USB
 * proof-of-concept stage:
 *
 *     HSE 8 MHz (crystal) -> PLL x9 -> 72 MHz  SYSCLK / HCLK
 *     USB = PLL / 1.5              -> 48 MHz
 *
 * If any step fails we hold in a tight loop; recovery is always hold-Up at
 * power-on to re-enter the DFU bootloader.
 */
extern "C" void SystemClock_Config(void) {
    RCC_OscInitTypeDef osc = {};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON; // crystal, not RCC_HSE_BYPASS
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    // On F3 the HSE->PLL prediv lives in the PLL sub-struct (there is no
    // top-level HSEPredivValue as on F4). HSE 8 MHz / 1 = 8 MHz into the PLL.
    osc.PLL.PREDIV = RCC_PREDIV_DIV1;
    osc.PLL.PLLMUL = RCC_PLL_MUL9; // 8 MHz * 9 = 72 MHz
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        while (true) {
        }
    }

    RCC_ClkInitTypeDef clk = {};
    clk.ClockType =
        RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1; // HCLK  = 72 MHz
    clk.APB1CLKDivider = RCC_HCLK_DIV2;  // PCLK1 = 36 MHz (APB1 max on F3)
    clk.APB2CLKDivider = RCC_HCLK_DIV1;  // PCLK2 = 72 MHz
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) {
        while (true) {
        }
    }

    RCC_PeriphCLKInitTypeDef periph = {};
    periph.PeriphClockSelection = RCC_PERIPHCLK_USB;
    periph.USBClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5; // 72 MHz / 1.5 = 48 MHz
    if (HAL_RCCEx_PeriphCLKConfig(&periph) != HAL_OK) {
        while (true) {
        }
    }
}
