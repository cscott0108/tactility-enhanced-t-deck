# SD Card App Layout

This folder contains ready-to-copy app folders for Tactility.

Quick use:
- Copy the folder `app/one.tactiity.uartrelay` to your device at `/sdcard/app/`
- Eject the SD card safely, insert it in the T-Deck, and reboot if needed
- Launch the app from the Tactility app list

Notes:
- The ELF binary is inside `app/one.tactiity.uartrelay/elf/esp32s3.elf`
- The `manifest.properties` file belongs at `app/one.tactiity.uartrelay/`
- Do not rename the `one.tactiity.uartrelay` folder
- For manual installs, you can copy only the `.elf` to `/sdcard/app/` but using the full folder is preferred

Troubleshooting:
- If the app does not appear, ensure the path is exactly `/sdcard/app/one.tactiity.uartrelay` with correct spelling
- Make sure the SD card is formatted FAT32 and writable
- Re-seat the SD card and reboot the device if the app list does not refresh
