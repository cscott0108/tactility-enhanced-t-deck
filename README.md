# Tactility Enhanced for T-Deck

**Full hardware support for LilyGO T-Deck with enhanced features**

## Overview

Tactility Enhanced is a custom distribution of [Tactility OS](https://github.com/ByteWelder/Tactility) specifically optimized for the LilyGO T-Deck, unlocking hardware features not available in the stock firmware.

### What Makes This Different?

This build enables **full T-Deck hardware utilization** that stock Tactility doesn't provide:

#### ✅ Working Features:
**USB Serial JTAG** - Bidirectional relay support (Rx + Tx)
**UART Relay App** - Monitor and control serial devices with USB/SD logging
**Firmware exports** - Extended symbol table for advanced app development
**GPS (GNSS)** - Built-in receiver supported
**I2C** - Stock I2C bus support
**Keyboard backlight control** - Brightness, timeout & wake-on-keypress
**Trackball navigation** - Full UI navigation with visual focus indicators & wake-on-use

#### 🚧 In Development:
- **WiFi file server** - Upload apps and files wirelessly
- **Web-based serial terminal** - Access UART over WiFi
- **4G LTE support** - Cellular connectivity
- **LoRa transceiver** - Long-range communication

### Key Applications

**UartRelay** - Flagship app for this distribution
- Bidirectional UART relay (device ↔ USB/WiFi)
- SD card logging with timestamps
- Real-time serial monitoring
- Configurable baud rates and protocols
- Future: GPS-tagged logs, remote access via 4G

![screenshot of UartRelay app](Documentation/pics/screenshot-UartRelay.png)

## Why This Fork Exists

The LilyGO T-Deck is packed with features (WiFi, 4G, GPS, LoRa, trackball, backlit keyboard) that aren't fully utilized by existing firmware options. This project aims to:

1. **Enable all hardware** - Every feature should work
2. **Professional tools** - Advanced debugging and development capabilities
3. **Field deployment** - Reliable remote access and monitoring
4. **Developer friendly** - Extended API for custom apps

## Hardware Support

**Fully Supported:**
- LilyGO T-Deck (all variants)
- ESP32-S3 with 8MB+ PSRAM

**Tested On:**
- LilyGO T-Deck v1.0

## Installation

### Prerequisites
- LilyGO T-Deck device
- USB-C cable
- SD card (recommended: 8GB+)
- **For UartRelay:** JST-PH 2.0mm 4-pin connector (HY2.0-4P) to access the onboard UART port for device monitoring

### Quick Start

1. **Download latest release**
	- Firmware: `Tactility-Enhanced-[version].bin`
	- Apps: `UartRelay.elf`
	- Direct app ELF: https://raw.githubusercontent.com/cscott0108/tactility-enhanced-t-deck/main/Prebuilt/SDCard/app/one.tactiity.uartrelay/elf/esp32s3.elf
	- SD card layout folder: https://github.com/cscott0108/tactility-enhanced-t-deck/tree/main/Prebuilt/SDCard/app/one.tactiity.uartrelay
	- Releases: https://github.com/cscott0108/tactility-enhanced-t-deck/releases

2. **Flash firmware** (Windows/Linux/Mac)
```bash
esptool.py --port COM3 write_flash 0x0 Tactility-Enhanced.bin
```

3. **Install apps**
	- Copy `.elf` files to SD card: `/sdcard/app/`
	- Or use built-in App Hub

4. **First boot**
	- Configure WiFi in Settings
	- Launch UartRelay from app list
	- Adjust keyboard backlight & timeout: Settings > Keyboard
	- (Optional) Change idle settings reload interval: Settings > Development
	- Test wake: let display/backlight timeout, press any key → both wake

### Building from Source

See [BUILDING.md](BUILDING.md) for detailed instructions.
```bash
# Clone repository
git clone https://github.com/cscott0108/tactility-enhanced-t-deck
cd tactility-enhanced-t-deck

# Build firmware
idf.py build

# Build apps
cd Apps/UartRelay
python3 tactility.py build esp32s3
```

## Roadmap

### Phase 1: Core Hardware ✅
* [x] USB Serial JTAG bidirectional
* [x] UART relay functionality
* [x] SD card logging
* [x] GPS (GNSS)
* [x] Keyboard backlight (brightness + timeout + wake)
* [x] Trackball navigation (encoder-based UI selection + wake)

### Phase 2: Connectivity 🚧
- [ ] WiFi file server
- [ ] Web-based serial terminal
- [ ] 4G LTE module support
- [ ] LoRa transceiver

### Phase 3: Integration 📋
- [ ] GPS-tagged UART logs
- [ ] Remote UART access (WiFi/4G)
- [ ] LoRa UART bridge
- [ ] Web dashboard
- [ ] MQTT integration

## Contributing

Contributions are welcome! This project aims to make the T-Deck reach its full potential.

**Areas where help is needed:**
- Testing on different T-Deck variants
- Documentation improvements
- New app development
- Hardware feature implementation

## Differences from Stock Tactility

| Feature | Stock Tactility | This Fork |
|---------|----------------|-----------|
| USB Bidirectional | ❌ | ✅ |
| Keyboard Backlight | ❌ | ✅ |
| Trackball Support | ❌ | ✅ |
| 4G LTE | ❌ | 📋 |
| GPS | ✅ | ✅ |
| LoRa | ❌ | 📋 |
| WiFi File Server | ❌ | 🚧 |
| Web Serial Terminal | ❌ | 📋 |
| UartRelay App | ❌ | ✅ |
| I2C | ✅ | ✅ |

Legend: ✅ Working | 🚧 In Progress | 📋 Planned | ❌ Not Available

## Recent Enhancements

**Unified Idle Management**
- Display + keyboard backlight idle logic merged into one service (reduced polling overhead)
- Configurable settings reload interval (default 10s) via Settings > Development

**Keyboard Backlight Improvements**
- Settings > Keyboard: enable, brightness, timeout enable, timeout value (5s–10min)
- Timeout & preferences persisted (NVS)
- Wake-on-keypress immediately restores display + keyboard backlights

**Stability & Boot Reliability**
- Deferred I2C initialization (keyboard & trackball) resolves early heap corruption
- Encoder-based trackball navigation with LVGL focus indicators for visual feedback
- NTP time persisted/restored before SNTP sync

**Developer Quality of Life**
- Idle reload interval user-adjustable (prompts reboot when changed)
- Reduced duplicated timers by consolidating services

## Troubleshooting

| Symptom | Resolution |
|---------|------------|
| Keyboard press doesn't wake display | Ensure timeout feature enabled; try a non-modifier key |
| Timeout value not saved | Leave Keyboard settings screen to trigger save |
| Idle interval not applied | Reboot after changing in Development settings |
| Early boot panic | Update to latest build (includes deferred I2C init) |

## Upgrading

1. Flash new firmware
2. (Optional) Clear outdated `/data/settings/` entries if behavior is inconsistent
3. Re-set keyboard timeout (Settings > Keyboard)
4. Adjust idle reload interval (Settings > Development) if needed

## Credits

**Based on [Tactility](https://github.com/ByteWelder/Tactility)** by Ken Van Hoeylandt (ByteWelder)

This is a community-driven fork focused on T-Deck hardware support. We maintain compatibility with the Tactility app ecosystem while extending hardware capabilities.

## License

[GNU General Public License Version 3](LICENSE.md)
