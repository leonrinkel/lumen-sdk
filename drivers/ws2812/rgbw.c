/*
 * Copyright (c) 2023 Leon Rinkel
 *
 * RGB to RGBW conversion according to Wang et al.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <math.h>

#include "rgbw.h"

void rgbw_conversion(
	/* outs: */ uint8_t* ro, uint8_t* go, uint8_t* bo, uint8_t* wo,
	/*  ins: */ uint8_t ri, uint8_t gi, uint8_t bi, uint8_t algo
)
{
	float m; /** min */
	float M; /** max */
	float w; /** white */
	float k; /** gain */
	float r; /** red */
	float g; /** green */
	float b; /** blue */

	if (ri == 0 && gi == 0 && bi == 0)
	{
		*ro = 0;
		*go = 0;
		*bo = 0;
		*wo = 0;
		return;
	}

	m = fmin(ri, fmin(gi, bi));
	M = fmax(ri, fmax(gi, bi));

	switch (algo)
	{
	case 1:
		w = m;
		break;
	case 2:
		w = pow(m, 2);
		break;
	case 3:
		w = -pow(m, 3) + pow(m, 2) + m;
		break;
	case 4:
		w = (m / M >= 0.5) ? M :
			(m * M) / (M - m);
		break;

	default:
		return;
	}

	k = (w + M) / M;

	r = k * ri - w;
	g = k * gi - w;
	b = k * bi - w;

	*wo = fmax(fmin(floor(w), 255), 0);
	*ro = fmax(fmin(floor(r), 255), 0);
	*go = fmax(fmin(floor(g), 255), 0);
	*bo = fmax(fmin(floor(b), 255), 0);
}
