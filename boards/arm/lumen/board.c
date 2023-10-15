/*
 * Copyright (c) 2018 Nordic Semiconductor ASA.
 * Copyright (c) 2023 Leon Rinkel
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/init.h>
#include <hal/nrf_power.h>

static int board_lumen_init(void)
{
	/* Configures VDD to be 3V instead of the usual 1.8V,
	 * this allows board to be programmed by DK without
	 * requiring external level shifting. */
	if (
		(nrf_power_mainregstatus_get(NRF_POWER) ==
		 	NRF_POWER_MAINREGSTATUS_HIGH) &&
		((NRF_UICR->REGOUT0 & UICR_REGOUT0_VOUT_Msk) ==
		 	(UICR_REGOUT0_VOUT_DEFAULT << UICR_REGOUT0_VOUT_Pos))
	)
	{
		NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos;
		while (NRF_NVMC->READY == NVMC_READY_READY_Busy);

		NRF_UICR->REGOUT0 =
			(NRF_UICR->REGOUT0 & ~((uint32_t)UICR_REGOUT0_VOUT_Msk)) |
			(UICR_REGOUT0_VOUT_3V0 << UICR_REGOUT0_VOUT_Pos);

		NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos;
		while (NRF_NVMC->READY == NVMC_READY_READY_Busy);

		NVIC_SystemReset();
	}

	return 0;
}

SYS_INIT(board_lumen_init, PRE_KERNEL_1,
	CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
