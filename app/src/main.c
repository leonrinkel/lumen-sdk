/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 * Copyright (c) 2023 Leon Rinkel
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys_clock.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>

#include <app_version.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL);

#define STRIP_NODE DT_ALIAS(led_strip)
#define STRIP_NUM_PIXELS DT_PROP(DT_ALIAS(led_strip), chain_length)
static const struct device* const strip = DEVICE_DT_GET(STRIP_NODE);

struct led_rgb pixels[STRIP_NUM_PIXELS] = {0};

/** Outputs colors transitioning red - green - blue.
 * Taken from https://github.com/adafruit/Adafruit_NeoPixel.
 */
void color_wheel(uint8_t pos, uint8_t* r, uint8_t* g, uint8_t* b)
{
	pos = 255 - pos;
	if (pos < 85)
	{
		*r = 255 - pos * 3;
		*g = 0;
		*b = pos * 3;
	}
	else if (pos < 170)
	{
		pos -= 85;
		*r = 0;
		*g = pos * 3;
		*b = 255 - pos * 3;
	}
	else
	{
		pos -= 170;
		*r = pos * 3;
		*g = 255 - pos * 3;
		*b = 0;
	}
}

int main(void)
{
	int err;
	int j = 0;
	k_timepoint_t till_heartbeat = sys_timepoint_calc(K_NO_WAIT);

	printk("lumen example application %s\n", APP_VERSION_STRING);

	if (!device_is_ready(strip)) {
		printk("led strip device is not ready\n");
		return 0;
	}

	while (1)
	{
		for (int i = 0; i < STRIP_NUM_PIXELS; i++)
		{
			color_wheel(
				((i * 256 / STRIP_NUM_PIXELS) + j) & 255,
				&(pixels[i].r), &(pixels[i].g), &(pixels[i].b)
			);
		}

		err = led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
		if (err < 0)
		{
			printk("unable to update led strip (err %d)\n", err);
		}

		if (++j >= 256 * 5)
		{
			j = 0;
		}

		if (sys_timepoint_expired(till_heartbeat))
		{
			printk("hello world i'm still here %llu\n",
				k_uptime_get());
			till_heartbeat = sys_timepoint_calc(K_SECONDS(1));
		}

		k_sleep(K_MSEC(20));
	}

	return 0;
}
