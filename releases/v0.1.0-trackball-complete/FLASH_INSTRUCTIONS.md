# Tactility Enhanced v0.1.0 - Trackball Complete

**Release Date:** November 20, 2025  
**Device:** LilyGO T-Deck

## Features in This Release

✅ **Complete Phase 1 Hardware Support:**
- USB Serial JTAG (bidirectional Rx/Tx)
- UART Relay App with SD logging
- GPS/GNSS support
- I2C bus support
- Keyboard backlight (brightness + timeout + wake-on-keypress)
- **Trackball navigation** (encoder-based UI selection with visual focus)

## Flashing Instructions

### Prerequisites
- ESP-IDF v5.5+ or esptool.py installed
- USB-C cable connected to T-Deck

### Method 1: Using esptool.py (Recommended)

```bash
esptool.py --chip esp32s3 --port COM3 --baud 921600 \
  --before default_reset --after hard_reset write_flash -z \
  --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x0 bootloader.bin \
  0x8000 partition-table.bin \
  0x10000 firmware.bin \
  0x410000 data.bin \
  0x810000 system.bin
```

**Note:** Replace `COM3` with your actual serial port (check Device Manager on Windows, or use `/dev/ttyACM0` on Linux/Mac).

### Method 2: Using ESP-IDF Flash Script (Windows PowerShell)

```powershell
# Navigate to this directory
cd releases\v0.1.0-trackball-complete

# Flash using Buildscripts helper
..\..\Buildscripts\Flashing\flash.ps1
```

### Method 3: Manual Flash with idf.py

```bash
# From project root with ESP-IDF environment loaded
idf.py -p COM3 flash
```

## Flash Memory Layout

| Address  | Partition         | File                  |
|----------|-------------------|-----------------------|
| 0x0      | Bootloader        | bootloader.bin        |
| 0x8000   | Partition Table   | partition-table.bin   |
| 0x10000  | Firmware          | firmware.bin          |
| 0x410000 | Data              | data.bin              |
| 0x810000 | System            | system.bin            |

## First Boot

After flashing:
1. Device will reboot automatically
2. Display shows Tactility boot logo
3. Main menu appears
4. Test trackball navigation (up/down/left/right to navigate, click to select)
5. Test keyboard backlight in Settings → Keyboard

## Troubleshooting

**Flash fails:**
- Hold `BOOT` button (GPIO 0) while connecting USB
- Try lower baud rate: `--baud 115200`
- Ensure correct COM port

**Device won't boot:**
- Check all partitions flashed successfully
- Try full chip erase: `esptool.py --chip esp32s3 erase_flash`
- Re-flash all partitions

**Features not working:**
- Factory reset: Settings → System → Factory Reset
- Check SD card formatted as FAT32
- Verify keyboard backlight in Settings

## Changelog

### New Features
- Full trackball navigation with encoder-based UI selection
- Visual focus indicators (outline + grow effect)
- Wake-on-trackball support
- Deferred I2C initialization (prevents early heap corruption)

### Improvements
- Keyboard backlight timeout configurable (5-600 seconds)
- Wake-on-keypress for keyboard
- Unified idle management (display + keyboard)
- Development settings for idle reload interval

## Known Issues
- WiFi file server not yet implemented
- Web serial terminal in development
- 4G LTE module not yet supported
- LoRa transceiver support pending

## Support

**Repository:** https://github.com/cscott0108/tactility-enhanced-t-deck  
**Base Project:** https://github.com/ByteWelder/Tactility  
**Issues:** https://github.com/cscott0108/tactility-enhanced-t-deck/issues

## License

See `LICENSE.md` in repository root.
