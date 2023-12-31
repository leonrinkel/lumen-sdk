/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 * Copyright (c) 2023 Leon Rinkel
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys_clock.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/settings/settings.h>

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

static const uint8_t gamma_correction[] =
{
	  0,   0,   0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   1,   1,   1,   1,
	  1,   1,   1,   1,   1,   1,   1,   1,
	  1,   2,   2,   2,   2,   2,   2,   2,
	  2,   3,   3,   3,   3,   3,   3,   3,
	  4,   4,   4,   4,   4,   5,   5,   5,
	  5,   6,   6,   6,   6,   7,   7,   7,
	  7,   8,   8,   8,   9,   9,   9,  10,
	 10,  10,  11,  11,  11,  12,  12,  13,
	 13,  13,  14,  14,  15,  15,  16,  16,
	 17,  17,  18,  18,  19,  19,  20,  20,
	 21,  21,  22,  22,  23,  24,  24,  25,
	 25,  26,  27,  27,  28,  29,  29,  30,
	 31,  32,  32,  33,  34,  35,  35,  36,
	 37,  38,  39,  39,  40,  41,  42,  43,
	 44,  45,  46,  47,  48,  49,  50,  50,
	 51,  52,  54,  55,  56,  57,  58,  59,
	 60,  61,  62,  63,  64,  66,  67,  68,
	 69,  70,  72,  73,  74,  75,  77,  78,
	 79,  81,  82,  83,  85,  86,  87,  89,
	 90,  92,  93,  95,  96,  98,  99, 101,
	102, 104, 105, 107, 109, 110, 112, 114,
	115, 117, 119, 120, 122, 124, 126, 127,
	129, 131, 133, 135, 137, 138, 140, 142,
	144, 146, 148, 150, 152, 154, 156, 158,
	160, 162, 164, 167, 169, 171, 173, 175,
	177, 180, 182, 184, 186, 189, 191, 193,
	196, 198, 200, 203, 205, 208, 210, 213,
	215, 218, 220, 223, 225, 228, 231, 233,
	236, 239, 241, 244, 247, 249, 252, 255
};

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
		pixels[i].r = gamma_correction[value[0]];
		pixels[i].g = gamma_correction[value[1]];
		pixels[i].b = gamma_correction[value[2]];
	}

	return len;
}

BT_GATT_SERVICE_DEFINE(lumen_svc,
	BT_GATT_PRIMARY_SERVICE(&lumen_uuid),
	BT_GATT_CHARACTERISTIC(&lumen_rgb_uuid.uuid,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE |
			BT_GATT_CHRC_WRITE_WITHOUT_RESP | BT_GATT_CHRC_INDICATE,
		BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT,
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
		LOG_WRN("connection failed (err %d)\n", err);
	}
	else
	{
		LOG_INF("connected\n");
	}
}

static void disconnected(struct bt_conn* conn, uint8_t reason)
{
	LOG_INF("disconnected (reason %d)\n", reason);
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

static void auth_passkey_display(struct bt_conn* conn, unsigned int passkey)
{
	char addr[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_INF("passkey for %s: %06u\n", addr, passkey);
}

static void auth_cancel(struct bt_conn* conn)
{
	char addr[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_INF("pairing cancelled for %s\n", addr);
}

static void pairing_complete(struct bt_conn* conn, bool bonded)
{
	char addr[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_INF("pairing completed for %s, bonded: %d\n", addr, bonded);
}

static void pairing_failed(struct bt_conn* conn, enum bt_security_err reason)
{
}

static struct bt_conn_auth_cb conn_auth_callbacks =
{
	.passkey_display = auth_passkey_display,
	.cancel = auth_cancel,
	.pairing_confirm = NULL,
};

static struct bt_conn_auth_info_cb conn_auth_info_callbacks =
{
	.pairing_complete = pairing_complete,
	.pairing_failed = pairing_failed,
};

int main(void)
{
	int err;
	const ssize_t device_id_len = 4;
	uint8_t device_id[device_id_len];
	ssize_t actual_device_id_len;
	uint32_t passkey;
	int j = 0;
	k_timepoint_t till_heartbeat = sys_timepoint_calc(K_NO_WAIT);

	LOG_INF("lumen example application %s\n", APP_VERSION_STRING);

	if (!device_is_ready(strip))
	{
		LOG_ERR("led strip device is not ready\n");
		return 0;
	}

	err = bt_conn_auth_cb_register(&conn_auth_callbacks);
	if (err < 0)
	{
		LOG_ERR("failed to register auth cb (err %d)\n", err);
		return 0;
	}

	err = bt_conn_auth_info_cb_register(&conn_auth_info_callbacks);
	if (err < 0)
	{
		LOG_ERR("failed to register auth info cb (err %d)\n", err);
		return 0;
	}

	actual_device_id_len =
		hwinfo_get_device_id(device_id, device_id_len);
	if (actual_device_id_len < 0)
	{
		LOG_ERR("failed to get device id (err %d)\n", err);
		return 0;
	}

	passkey = (
			(uint32_t) (
				(device_id[3] << 24) |
				(device_id[2] << 16) |
				(device_id[1] <<  8) |
				(device_id[0] <<  0)
			)
		) % 1000000;
	LOG_INF("setting passkey to %06u\n", passkey);
	bt_passkey_set(passkey);

	err = bt_enable(NULL);
	if (err < 0)
	{
		LOG_ERR("bluetooth enable failed (err %d)\n", err);
		return 0;
	}
	LOG_INF("bluetooth enabled\n");

	if (IS_ENABLED(CONFIG_SETTINGS))
	{
		settings_load();
	}

	err = bt_le_adv_start(BT_LE_ADV_CONN_NAME,
		ad, ARRAY_SIZE(ad), NULL, 0);
	if (err < 0)
	{
		LOG_ERR("failed to start advertising (err %d)\n", err);
		return 0;
	}
	LOG_INF("started advertising\n");

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
			LOG_WRN("unable to update led strip (err %d)\n", err);
		}

		if (do_color_wheel && ++j >= 256 * 5)
		{
			j = 0;
		}

		if (sys_timepoint_expired(till_heartbeat))
		{
			LOG_INF("hello world i'm still here %llu\n",
				k_uptime_get());
			till_heartbeat = sys_timepoint_calc(K_SECONDS(1));
		}

		k_sleep(K_MSEC(20));
	}

	return 0;
}
