# MazarbuLib-ESP32-Sample

A demo project that runs [MazarbuLib](https://github.com/reboot-required/MazarbuLib)
on an **ESP32-WROOM-32** using the Arduino framework and PlatformIO. It displays
two live data screens over the USB-serial monitor and lets you switch between
them by typing `n` (next) or `p` (previous) in the monitor.

## Screens

| Screen | Rows |
|--------|------|
| **System Info** | Uptime (s), Free Heap (B), Chip Temp (C) |
| **GPIO Status** | GPIO2 (LED), GPIO4, GPIO5 |

## Hardware

- ESP32-WROOM-32 (any ESP32 dev board matching the `esp32dev` PlatformIO board ID)
- GPIO2, GPIO4, GPIO5 configured as digital inputs

## Prerequisites

- [PlatformIO](https://platformio.org/) CLI or IDE extension
- Git (to initialise the submodule)

## Getting Started

```bash
# Clone with submodule
git clone --recurse-submodules https://github.com/reboot-required/MazarbuLib-ESP32-Sample
cd MazarbuLib-ESP32-Sample

# Or, if already cloned without --recurse-submodules:
git submodule update --init

# Build
pio run

# Flash and open serial monitor (115200 baud)
pio run --target upload && pio device monitor
```

## Project Structure

```
MazarbuLib-ESP32-Sample/
├── lib/
│   └── mazarbulib/          # MazarbuLib git submodule
│       ├── include/
│       │   ├── mazarbulib.h
│       │   └── mazarbulib_config.h
│       ├── src/
│       │   └── mazarbulib.c
│       └── library.json     # PlatformIO library manifest
├── src/
│   └── main.cpp             # Application entry point
└── platformio.ini
```

PlatformIO auto-discovers `lib/mazarbulib/` via its `library.json`; no
`lib_deps` entry is required in `platformio.ini`.

## Serial Navigation

With the serial monitor open at **115200 baud**:

| Key | Action |
|-----|--------|
| `n` | Next screen |
| `p` | Previous screen |

## License

MIT — see [LICENSE](LICENSE).
