// Copyright (c) 2026 Lukas Kraft
// https://github.com/reboot-required
//
// MazarbuLib-ESP32-Sample — demonstrates MazarbuLib on ESP32-WROOM-32.
// Named after the Book of Mazarbul from J.R.R. Tolkien's writings.
//
// SPDX-License-Identifier: MIT

#include <Arduino.h>
#include <esp_system.h>

#include "mazarbulib.h"

static mazarbulib_t g_lib;

static uint32_t g_uptime_s  = 0;
static uint32_t g_free_heap = 0;
static float    g_chip_temp = 0.0f;
static bool     g_gpio2     = false;
static bool     g_gpio4     = false;
static bool     g_gpio5     = false;

static void uart_send(const char *data, size_t len) {
  Serial.write(reinterpret_cast<const uint8_t *>(data), len);
}

static void terminal_clear(void) {
  Serial.print("\033[2J\033[H");
}

void setup() {
  Serial.begin(115200);

  pinMode(2, INPUT);
  pinMode(4, INPUT);
  pinMode(5, INPUT);

  mazarbulib_init(&g_lib, uart_send, terminal_clear);

  int s0 = mazarbulib_register_screen(&g_lib, "System Info");
  mazarbulib_register_row(&g_lib, s0, "Uptime (s)",   MAZARBULIB_TYPE_UINT32, &g_uptime_s);
  mazarbulib_register_row(&g_lib, s0, "Free Heap (B)", MAZARBULIB_TYPE_UINT32, &g_free_heap);
  mazarbulib_register_row(&g_lib, s0, "Chip Temp (C)", MAZARBULIB_TYPE_FLOAT,  &g_chip_temp);

  int s1 = mazarbulib_register_screen(&g_lib, "GPIO Status");
  mazarbulib_register_row(&g_lib, s1, "GPIO2 (LED)", MAZARBULIB_TYPE_BOOL, &g_gpio2);
  mazarbulib_register_row(&g_lib, s1, "GPIO4",       MAZARBULIB_TYPE_BOOL, &g_gpio4);
  mazarbulib_register_row(&g_lib, s1, "GPIO5",       MAZARBULIB_TYPE_BOOL, &g_gpio5);
}

void loop() {
  g_uptime_s  = millis() / 1000UL;
  g_free_heap = esp_get_free_heap_size();
  g_chip_temp = temperatureRead();
  g_gpio2     = digitalRead(2);
  g_gpio4     = digitalRead(4);
  g_gpio5     = digitalRead(5);

  while (Serial.available()) {
    mazarbulib_feed_char(&g_lib, static_cast<char>(Serial.read()));
  }

  static uint32_t last_tick = 0;
  uint32_t now = millis();
  if (now - last_tick >= 1000) {
    last_tick = now;
    mazarbulib_tick(&g_lib);
  }
}
