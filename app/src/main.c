/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 * Copyright (c) 2023 Leon Rinkel
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys_clock.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include <app_version.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL);

#define STRIP_NODE DT_ALIAS(led_strip)
#define STRIP_NUM_PIXELS DT_PROP(DT_ALIAS(led_strip), chain_length)
static const struct device* const strip = DEVICE_DT_GET(STRIP_NODE);

struct led_rgb pixels[STRIP_NUM_PIXELS] = {0};
static int do_color_wheel = 1;

#define BT_UUID_LUMEN_SERVICE_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0)
static struct bt_uuid_128 lumen_uuid =
	BT_UUID_INIT_128(BT_UUID_LUMEN_SERVICE_VAL);
static struct bt_uuid_128 lumen_rgb_uuid = BT_UUID_INIT_128(
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1));

#define RGB_MAX_LEN 3
static uint8_t rgb_value[RGB_MAX_LEN] = {0};

static ssize_t read_rgb(struct bt_conn* conn, const struct bt_gatt_attr* attr,
	void* buf, uint16_t len, uint16_t offset)
{
	const uint8_t* value = attr->user_data;
	return bt_gatt_attr_read(
		conn, attr, buf, len, offset, value, RGB_MAX_LEN);
}

static ssize_t write_rgb(struct bt_conn* conn, const struct bt_gatt_attr* attr,
	const void* buf, uint16_t len, uint16_t offset, uint8_t flags)
{
	uint8_t* value = attr->user_data;

	if (len != RGB_MAX_LEN)
	{
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}
	else if (offset != 0)
	{
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	if (do_color_wheel)
	{
		do_color_wheel = 0;
	}

	value[0] = ((const uint8_t*) buf)[0];
	value[1] = ((const uint8_t*) buf)[1];
	value[2] = ((const uint8_t*) buf)[2];

	for (int i = 0; i < STRIP_NUM_PIXELS; i++)
	{
		pixels[i].r = value[0];
		pixels[i].g = value[1];
		pixels[i].b = value[2];
	}

	return len;
}

BT_GATT_SERVICE_DEFINE(lumen_svc,
	BT_GATT_PRIMARY_SERVICE(&lumen_uuid),
	BT_GATT_CHARACTERISTIC(&lumen_rgb_uuid.uuid,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_INDICATE,
		BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
		read_rgb, write_rgb, rgb_value
	),
);

static const struct bt_data ad[] =
{
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_LUMEN_SERVICE_VAL),
};

static void connected(struct bt_conn* conn, uint8_t err)
{
	if (err)
	{
		printk("connection failed (err %d)\n", err);
	}
	else
	{
		printk("connected\n");
	}
}

static void disconnected(struct bt_conn* conn, uint8_t reason)
{
	printk("disconnected (reason %d)\n", reason);
}

BT_CONN_CB_DEFINE(conn_callbacks) =
{
	.connected = connected,
	.disconnected = disconnected,
};

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

	if (!device_is_ready(strip))
	{
		printk("led strip device is not ready\n");
		return 0;
	}

	err = bt_enable(NULL);
	if (err < 0)
	{
		printk("bluetooth enable failed (err %d)\n", err);
		return 0;
	}
	printk("bluetooth enabled\n");

	err = bt_le_adv_start(BT_LE_ADV_CONN_NAME,
		ad, ARRAY_SIZE(ad), NULL, 0);
	if (err < 0)
	{
		printk("failed to start advertising (err %d)\n", err);
		return 0;
	}
	printk("started advertising\n");

	while (1)
	{
		if (do_color_wheel)
		{
			for (int i = 0; i < STRIP_NUM_PIXELS; i++)
			{
				color_wheel(
					((i * 256 / STRIP_NUM_PIXELS) + j) & 255,
					&(pixels[i].r), &(pixels[i].g), &(pixels[i].b)
				);
			}
		}

		err = led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
		if (err < 0)
		{
			printk("unable to update led strip (err %d)\n", err);
		}

		if (do_color_wheel && ++j >= 256 * 5)
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
