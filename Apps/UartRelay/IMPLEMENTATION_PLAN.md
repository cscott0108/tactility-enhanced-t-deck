# UART Relay Implementation Plan

## Phase 1: USB CDC Forwarding (PRIORITY)

### ESP-IDF USB Serial JTAG VFS
The T-Deck USB-C port uses USB Serial JTAG, which has VFS support.
We can write to it using standard C file operations!

**Implementation:**
```cpp
// In ConsoleView.h, add member:
FILE* usbSerial = nullptr;

// In startLogic():
usbSerial = fopen("/dev/usb", "w");
if (usbSerial != nullptr) {
    setvbuf(usbSerial, NULL, _IONBF, 0); // No buffering for immediate relay
}

// In uartThreadMain() after reading byte:
if (success && usbSerial != nullptr) {
    fwrite(&byte, 1, 1, usbSerial);
}

// In stopLogic():
if (usbSerial != nullptr) {
    fclose(usbSerial);
    usbSerial = nullptr;
}
```

## Phase 2: SD Card Logging (SECONDARY)

### SD Card Access
Use standard C file I/O - ESP-IDF mounts SD at `/sdcard`

**Implementation:**
```cpp
// Add members:
FILE* logFile = nullptr;
bool loggingEnabled = false;
lv_obj_t* logToggleSwitch = nullptr;

// Add UI toggle button in startViews()
// When enabled:
char filename[64];
snprintf(filename, sizeof(filename), "/sdcard/uart_log_%ld.txt", time(NULL));
logFile = fopen(filename, "w");

// In uartThreadMain():
if (success && loggingEnabled && logFile != nullptr) {
    fwrite(&byte, 1, 1, logFile);
    // Periodic flush every N bytes or time
}
```

## Phase 3: Status Indicators

Add status display:
- USB connection status
- SD logging status  
- Bytes relayed count
- Log file path

## Testing Steps

1. **Test USB forwarding only**
   - Connect T-Deck to computer
   - Open serial monitor (screen /dev/ttyACM0 115200)
   - Send data to Grove UART
   - Verify it appears on computer

2. **Test SD logging**
   - Enable logging via UI
   - Send data
   - Check /sdcard/ for log file

3. **Test simultaneous**
   - Both USB + SD should work together

