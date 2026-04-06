# MazarbuLib-ESP32-Sample

A demo project that runs [MazarbuLib](https://github.com/reboot-required/MazarbuLib)
on an **ESP32-WROOM-32** using the Arduino framework and PlatformIO. Two
build environments demonstrate opposite ends of the library's feature set.
Switch between screens at any time by typing `n` (next) or `p` (previous)
in the serial monitor.

## Examples

### `simple` — GPIO & chip basics

A minimal starting point: three screens, each row a different value type.

| Screen | Rows |
|--------|------|
| **System Info** | Uptime (s), Free Heap (B), Chip Temp (C) |
| **GPIO Status** | GPIO2 (LED), GPIO4, GPIO5 |

Requires GPIO2, GPIO4, GPIO5 wired as digital inputs.

### `advanced` — full-table ESP32 showcase

Four screens packed with live peripheral data, demonstrating large tables,
all supported value types, and asynchronous WiFi scanning.

| Screen | Rows | Description |
|--------|------|-------------|
| **System Info** | 14 | Heap stats, chip temp, CPU freq, flash/PSRAM sizes, IDF version, reset reason, FreeRTOS task count |
| **WiFi Networks 1–4** | 16 | SSID, RSSI (dBm), channel, security type for the top 4 scanned APs |
| **WiFi Networks 5–8** | 16 | Same for APs 5–8 |
| **WiFi Scanner** | 4 | Scan status, network count, scan iterations, timestamp of last scan |

Network slots show `---` until the first scan completes. Scans repeat every
15 s automatically (5 s retry on failure). No `delay()` in the main loop.

## Hardware

- ESP32-WROOM-32 (any board matching the `esp32dev` PlatformIO board ID)
- `simple` only: GPIO2, GPIO4, GPIO5 as digital inputs

## Prerequisites

- [PlatformIO](https://platformio.org/) CLI or IDE extension

## Getting Started

```bash
# Clone
git clone https://github.com/reboot-required/MazarbuLib-ESP32-Sample
cd MazarbuLib-ESP32-Sample

# Initialize submodules
git submodule update --init

# Build simple example
pio run -e simple

# Flash simple example and open serial monitor (115200 baud)
pio run -e simple --target upload && pio device monitor

# Build advanced example
pio run -e advanced

# Flash advanced example
pio run -e advanced --target upload && pio device monitor
```

## Project Structure

```
MazarbuLib-ESP32-Sample/
├── src/
│   ├── main.cpp              # simple environment
│   └── main_advanced.cpp     # advanced environment
└── platformio.ini
```

`platformio.ini` pins MazarbuLib directly from GitHub via `lib_deps`, so the
project builds without a local Git submodule or SSH access.

## Serial Navigation

With the serial monitor open at **115200 baud**:

| Key | Action |
|-----|--------|
| `n` | Next screen |
| `p` | Previous screen |

## License

MIT — see [LICENSE](LICENSE).
