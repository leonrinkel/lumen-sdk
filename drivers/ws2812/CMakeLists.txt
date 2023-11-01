# SPDX-License-Identifier: Apache-2.0

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_LUMEN_WS2812_STRIP_GPIO ws2812_gpio.c)
zephyr_library_sources_ifdef(CONFIG_LUMEN_WS2812_STRIP_SPI  ws2812_spi.c)
zephyr_library_sources_ifdef(CONFIG_LUMEN_WS2812_STRIP_I2S  ws2812_i2s.c)
