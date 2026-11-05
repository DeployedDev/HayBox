#ifndef _FRAME1_PINFINDER_HPP
#define _FRAME1_PINFINDER_HPP

/*
 * GameCube data-pin finder for the Frame1. Defined only in the diagnostic build
 * (`pio run -e frame1_pinfinder`, which sets -D FRAME1_PINFINDER); called from
 * setup() and never returns. See pinfinder.cpp.
 */
void frame1_pinfinder();

#endif
