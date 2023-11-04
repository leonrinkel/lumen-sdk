#ifndef LUMEN_WS2812_RGBW_H
#define LUMEN_WS2812_RGBW_H

#include <stdint.h>

void rgbw_conversion(
	/* outs: */ uint8_t* ro, uint8_t* go, uint8_t* bo, uint8_t* wo,
	/*  ins: */ uint8_t ri, uint8_t gi, uint8_t bi, uint8_t algo
);

#endif /* LUMEN_WS2812_RGBW_H */
