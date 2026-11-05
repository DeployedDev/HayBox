#ifndef _HAL_STDLIB_HPP
#define _HAL_STDLIB_HPP

#include <Arduino.h>

/* The Pico SDK defines `uint`, which HayBox's shared code uses; STM32duino does
 * not, so provide it here. Guarded in case a core header already defines it. */
#ifndef _HAVE_UINT_TYPEDEF
#define _HAVE_UINT_TYPEDEF
typedef unsigned int uint;
#endif

#endif
