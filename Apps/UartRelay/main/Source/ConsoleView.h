#pragma once

#include "View.h"
#include "esp_log.h"
#include "driver/usb_serial_jtag.h"

#if __has_include(<hal/usb_serial_jtag_ll.h>)
#include <hal/usb_serial_jtag_ll.h>
#define TT_HAS_USB_SERIAL_JTAG_LL 1
#else
#define TT_HAS_USB_SERIAL_JTAG_LL 0
#endif

#include <Str.h>
#include <sstream>
#include <lvgl.h>
#include <memory>
#include <stdio.h>
#include <time.h>

#include <tt_lvgl.h>
#include <tt_thread.h>

#include <TactilityCpp/Mutex.h>
#include <TactilityCpp/Thread.h>
#include <TactilityCpp/LvglLock.h>
#include <TactilityCpp/Preferences.h>

constexpr size_t receiveBufferSize = 512;
constexpr size_t renderBufferSize = receiveBufferSize + 2;

class ConsoleView final : public View {

    const char* TAG = "UartRelay";

    lv_obj_t* _Nullable parent = nullptr;
    lv_obj_t* _Nullable logTextarea = nullptr;
    lv_obj_t* _Nullable inputTextarea = nullptr;
    
    std::shared_ptr<Uart> _Nullable uart = nullptr;
    std::shared_ptr<Thread> uartThread _Nullable = nullptr;
    bool uartThreadInterrupted = false;
    std::shared_ptr<Thread> usbThread _Nullable = nullptr;
    bool usbThreadInterrupted = false;
    std::shared_ptr<Thread> viewThread _Nullable = nullptr;
    bool viewThreadInterrupted = false;
    
    Mutex mutex = Mutex(MutexTypeRecursive);
    uint8_t receiveBuffer[receiveBufferSize];
    uint8_t renderBuffer[renderBufferSize];
    size_t receiveBufferPosition = 0;
    Str terminatorString = "\n";
    
    // USB Relay
    // We no longer rely on stdio FILE* for relay I/O (to allow log suppression separately).
    FILE* usbSerial = nullptr;  // legacy handle (unused for write path)
    FILE* usbInput = nullptr;   // stdin for reading (Tx relay)
    bool usbRelayEnabled = false;
    bool localEchoEnabled = true; // echo USB-typed characters into the view immediately
        // Log suppression when relay active
        vprintf_like_t previousVprintf = nullptr;

        static int stub_vprintf(const char* fmt, va_list ap) {
            // Suppress all log output while relay active
            return 0; // Indicate nothing written
        }

        void enableLogSuppression() {
            if (previousVprintf == nullptr) {
                previousVprintf = esp_log_set_vprintf(stub_vprintf);
            }
        }

        void disableLogSuppression() {
            if (previousVprintf != nullptr) {
                esp_log_set_vprintf(previousVprintf);
                previousVprintf = nullptr;
            }
        }
    
    // SD Card Logging
    FILE* sdLogFile = nullptr;
    bool sdLoggingEnabled = false;
    char sdLogFilePath[64];

    LvglLock lvglLock;
    Preferences prefs = Preferences("UartRelay");

    // Batching buffers
    static constexpr size_t usbTxBatchSize = 128; // UART->USB batch size
    uint8_t usbTxBatch[usbTxBatchSize];
    size_t usbTxCount = 0;
    TickType_t usbTxLastFlush = 0;

    bool isUartThreadInterrupted() const {
        auto lock = mutex.asScopedLock();
        lock.lock();
        return uartThreadInterrupted;
    }

    bool isViewThreadInterrupted() const {
        auto lock = mutex.asScopedLock();
        lock.lock();
        return viewThreadInterrupted;
    }

    bool isUsbThreadInterrupted() const {
        auto lock = mutex.asScopedLock();
        lock.lock();
        return usbThreadInterrupted;
    }

    bool openUsbRelay() {
        // Initialize USB Serial JTAG driver for reading
        usb_serial_jtag_driver_config_t usb_config = {
            .tx_buffer_size = 256,
            .rx_buffer_size = 256,
        };
        
        // Install driver if not already installed
        esp_err_t err = usb_serial_jtag_driver_install(&usb_config);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "Failed to install USB Serial JTAG driver: %d", err);
        }
        
        // Open stdin for reading (Tx relay: USB → Grove)
        usbInput = stdin; // Used only to gate driver install success path

        ESP_LOGI(TAG, "USB relay opened (driver installed)");
        return true;
    }

    void closeUsbRelay() {
        // Don't close stdout/stdin, just stop using them
        usbSerial = nullptr;
        usbInput = nullptr;
        
        // Uninstall USB Serial JTAG driver
        // Note: This may fail if other parts of firmware are using it
        // That's okay - we just stop reading from it
        usb_serial_jtag_driver_uninstall();
        
        ESP_LOGI(TAG, "USB relay closed");
    }

    bool openSdLog() {
        // Generate filename with timestamp (simple epoch-based)
        time_t now = time(nullptr);
        snprintf(sdLogFilePath, sizeof(sdLogFilePath), 
                "/sdcard/uart_log_%ld.txt", (long)now);
        
        FILE* file = fopen(sdLogFilePath, "w");
        if (file != nullptr) {
            fflush(file); // Ensure file is created on disk
            mutex.lock();
            sdLogFile = file;
            mutex.unlock();
            ESP_LOGI(TAG, "SD log opened: %s", sdLogFilePath);
            return true;
        } else {
            ESP_LOGE(TAG, "Failed to open SD log at %s (errno: %d)", sdLogFilePath, errno);
            return false;
        }
    }

    void closeSdLog() {
        mutex.lock();
        FILE* file = sdLogFile;
        sdLogFile = nullptr;
        mutex.unlock();
        
        if (file != nullptr) {
            fclose(file);
            ESP_LOGI(TAG, "SD log closed: %s", sdLogFilePath);
        }
    }

    void updateViews() {
        auto scoped_lvgl_lock = lvglLock.asScopedLock();
        if (!scoped_lvgl_lock.lock()) {
            return;
        }

        if (parent == nullptr) {
            return;
        }

        if (mutex.lock()) {
            size_t first_part_size = receiveBufferSize - receiveBufferPosition;
            memcpy(renderBuffer, receiveBuffer + receiveBufferPosition, first_part_size);
            renderBuffer[receiveBufferPosition] = '\n';
            if (receiveBufferPosition > 0) {
                memcpy(renderBuffer + first_part_size + 1, receiveBuffer, (receiveBufferSize - first_part_size));
                renderBuffer[receiveBufferSize - 1] = 0x00;
            }
            mutex.unlock();
        }

        tt_lvgl_lock(TT_MAX_TICKS);
        lv_textarea_set_text(logTextarea, (const char*)renderBuffer);
        tt_lvgl_unlock();
    }

    int32_t viewThreadMain() {
        while (!isViewThreadInterrupted()) {
            auto start_time = tt_kernel_get_ticks();

            updateViews();

            auto end_time = tt_kernel_get_ticks();
            auto time_diff = end_time - start_time;
            if (time_diff < 500U) {
                tt_kernel_delay_ticks((500U - time_diff) / portTICK_PERIOD_MS);
            }
        }

        return 0;
    }

    static int32_t viewThreadMainStatic(void* context) {
        auto* self = static_cast<ConsoleView*>(context);
        return self->viewThreadMain();
    }

    int32_t uartThreadMain() {
        char byte;

        while (!isUartThreadInterrupted()) {
            assert(uart != nullptr);
            bool success = uart->readByte(&byte, 50 / portTICK_PERIOD_MS);

            if (isUartThreadInterrupted()) {
                break;
            }

            if (success) {
                // Forward to USB if enabled
                if (usbRelayEnabled) {
                    // Accumulate into batch and send in chunks
                    usbTxBatch[usbTxCount++] = static_cast<uint8_t>(byte);
                    TickType_t now = tt_kernel_get_ticks();
                    bool timeToFlush = (now - usbTxLastFlush) > (20 / portTICK_PERIOD_MS);
                    bool isNewline = (byte == '\n' || byte == '\r');
                    if (usbTxCount >= usbTxBatchSize || timeToFlush || isNewline) {
                        size_t offset = 0;
                        while (offset < usbTxCount) {
                            size_t to_send = usbTxCount - offset;
                            int written = usb_serial_jtag_write_bytes(usbTxBatch + offset, to_send, 20 / portTICK_PERIOD_MS);
                            if (written <= 0) {
                                break; // give up this cycle
                            }
                            offset += static_cast<size_t>(written);
                        }
#if TT_HAS_USB_SERIAL_JTAG_LL
                        usb_serial_jtag_ll_txfifo_flush();
#endif
                        usbTxCount = 0;
                        usbTxLastFlush = now;
                    }
                }
    
                // Store in display buffer and log to SD card if enabled
                mutex.lock();
                if (sdLoggingEnabled && sdLogFile != nullptr) {
                    fwrite(&byte, 1, 1, sdLogFile);
                    // Flush periodically (every 512 bytes handled by circular buffer)
                }
                receiveBuffer[receiveBufferPosition++] = byte;
                if (receiveBufferPosition == receiveBufferSize) {
                    receiveBufferPosition = 0;
                    // Flush SD log when buffer wraps
                    if (sdLoggingEnabled && sdLogFile != nullptr) {
                        fflush(sdLogFile);
                    }
                }
                mutex.unlock();
            }

        }

        return 0;
    }

    static int32_t uartThreadMainStatic(void* view) {
        auto* self = static_cast<ConsoleView*>(view);
        return self->uartThreadMain();
    }

    int32_t usbThreadMain() {
        const size_t buf_size = 128;
        uint8_t buffer[buf_size];

        while (!isUsbThreadInterrupted()) {
            if (usbInput != nullptr) {
                // Read from USB Serial JTAG using ESP32 driver
                int len = usb_serial_jtag_read_bytes(buffer, buf_size, 100 / portTICK_PERIOD_MS);
                
                if (isUsbThreadInterrupted()) {
                    break;
                }
                
                if (len > 0) {
                    // Forward all received bytes to UART
                    if (uart != nullptr) {
                        size_t offset = 0;
                        while (offset < static_cast<size_t>(len)) {
                            size_t to_send = static_cast<size_t>(len) - offset;
                            size_t sent = uart->writeBytes((const char*)buffer + offset, to_send, 50 / portTICK_PERIOD_MS);
                            if (sent == 0) {
                                // avoid tight loop
                                tt_kernel_delay_ticks(5 / portTICK_PERIOD_MS);
                                break;
                            }
                            offset += sent;
                        }
                    }

                    // Local echo into view buffer for responsiveness
                    if (localEchoEnabled) {
                        auto lock = mutex.asScopedLock();
                        if (lock.lock()) {
                            for (int i = 0; i < len; i++) {
                                receiveBuffer[receiveBufferPosition++] = buffer[i];
                                if (receiveBufferPosition == receiveBufferSize) {
                                    receiveBufferPosition = 0;
                                }
                            }
                        }
                    }
                }
            } else {
                tt_kernel_delay_ticks(50 / portTICK_PERIOD_MS);
            }
        }

        return 0;
    }

    static int32_t usbThreadMainStatic(void* view) {
        auto* self = static_cast<ConsoleView*>(view);
        return self->usbThreadMain();
    }

    static void onSendClickedCallback(lv_event_t* event) {
        auto* view = (ConsoleView*)lv_event_get_user_data(event);
        view->onSendClicked();
    }

    static void onTerminatorDropdownValueChangedCallback(lv_event_t* event) {
        auto* view = (ConsoleView*)lv_event_get_user_data(event);
        view->onTerminatorDropDownValueChanged(event);
    }

    void onTerminatorDropDownValueChanged(lv_event_t* event) {
        auto* dropdown = static_cast<lv_obj_t*>(lv_event_get_target(event));
        mutex.lock();
        switch (lv_dropdown_get_selected(dropdown)) {
            case 0:
                terminatorString = "\n";
                break;
            case 1:
                terminatorString = "\r\n";
                break;
        }
        mutex.unlock();
    }

    void onSendClicked() {
        mutex.lock();
        Str input_text = lv_textarea_get_text(inputTextarea);
        Str to_send;
        to_send.appendf("%s%s", input_text.c_str(), terminatorString.c_str());
        mutex.unlock();

        if (uart != nullptr) {
            if (!uart->writeBytes(to_send.c_str(), to_send.length(), 100 / portTICK_PERIOD_MS)) {
                ESP_LOGE(TAG, "Failed to send \"%s\"", input_text.c_str());
            }
        }

        lv_textarea_set_text(inputTextarea, "");
    }

public:

    void startLogic(std::unique_ptr<Uart> newUart, bool enableUsbRelay, bool enableSdLog) {
        memset(receiveBuffer, 0, receiveBufferSize);

        assert(uartThread == nullptr);
        assert(uart == nullptr);

        uart = std::move(newUart);
        
        // Apply settings from parameters (not from saved preferences)
        usbRelayEnabled = enableUsbRelay;
        sdLoggingEnabled = enableSdLog;
        
        // Open USB relay if requested
        if (usbRelayEnabled) {
            openUsbRelay();
            enableLogSuppression();
        }
        
        // Open SD log if requested
        if (sdLoggingEnabled) {
            if (!openSdLog()) {
                ESP_LOGE(TAG, "Failed to open SD log file");
                sdLoggingEnabled = false;
            }
        }
        
        uartThreadInterrupted = false;
        uartThread = std::make_unique<Thread>(
            "UartRelayUart",
            4096,
            uartThreadMainStatic,
            this
        );
        uartThread->setPriority(ThreadPriorityHigh);
        uartThread->start();
        
        // Start USB read thread if USB relay is enabled (for Tx relay)
        if (usbRelayEnabled) {
            usbThreadInterrupted = false;
            usbThread = std::make_unique<Thread>(
                "UartRelayUsb",
                4096,
                usbThreadMainStatic,
                this
            );
            usbThread->setPriority(ThreadPriorityHigh);
            usbThread->start();
            ESP_LOGI(TAG, "USB read thread started for Tx relay");
        }
    }

    void startViews(lv_obj_t* parent) {
        this->parent = parent;

        lv_obj_set_style_pad_gap(parent, 2, 0);

        // Log display - FULL SCREEN (removed status label and control panel)
        logTextarea = lv_textarea_create(parent);
        lv_textarea_set_placeholder_text(logTextarea, "Waiting for data...");
        lv_obj_set_flex_grow(logTextarea, 1);
        lv_obj_set_width(logTextarea, LV_PCT(100));
        lv_obj_add_state(logTextarea, LV_STATE_DISABLED);
        lv_obj_set_style_margin_ver(logTextarea, 0, 0);

        // Input panel - SMALL at bottom
        auto* input_wrapper = lv_obj_create(parent);
        lv_obj_set_size(input_wrapper, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_pad_all(input_wrapper, 0, 0);
        lv_obj_set_style_border_width(input_wrapper, 0, 0);
        lv_obj_set_width(input_wrapper, LV_PCT(100));
        lv_obj_set_flex_flow(input_wrapper, LV_FLEX_FLOW_ROW);

        inputTextarea = lv_textarea_create(input_wrapper);
        lv_textarea_set_one_line(inputTextarea, true);
        lv_textarea_set_placeholder_text(inputTextarea, "Text to send");
        lv_obj_set_width(inputTextarea, LV_PCT(100));
        lv_obj_set_flex_grow(inputTextarea, 1);

        auto* terminator_dropdown = lv_dropdown_create(input_wrapper);
        lv_dropdown_set_options(terminator_dropdown, "\\n\n\\r\\n");
        lv_obj_set_width(terminator_dropdown, 70);
        lv_obj_add_event_cb(terminator_dropdown, onTerminatorDropdownValueChangedCallback, LV_EVENT_VALUE_CHANGED, this);

        auto* button = lv_button_create(input_wrapper);
        auto* button_label = lv_label_create(button);
        lv_label_set_text(button_label, "Send");
        lv_obj_add_event_cb(button, onSendClickedCallback, LV_EVENT_SHORT_CLICKED, this);

        viewThreadInterrupted = false;
        viewThread = std::make_unique<Thread>(
            "UartRelayView",
            4096,
            viewThreadMainStatic,
            this
        );
        viewThread->setPriority(ThreadPriorityHigher);
        viewThread->start();
    }

    void stopLogic() {
        auto lock = mutex.asScopedLock();
        lock.lock();

        uartThreadInterrupted = true;
        usbThreadInterrupted = true;

        auto old_uart_thread = std::move(uartThread);
        auto old_usb_thread = std::move(usbThread);
        lock.unlock();

        if (old_uart_thread != nullptr && old_uart_thread->getState() != ThreadStateStopped) {
            old_uart_thread->join();
        }
        
        if (old_usb_thread != nullptr && old_usb_thread->getState() != ThreadStateStopped) {
            old_usb_thread->join();
        }
        
        // Always attempt to close USB relay and restore log suppression
        closeUsbRelay();
        disableLogSuppression();
        usbRelayEnabled = false;
        
        // Always attempt to close SD log
        closeSdLog();
        sdLoggingEnabled = false;
    }

    void stopViews() {
        auto lock = mutex.asScopedLock();
        lock.lock();

        viewThreadInterrupted = true;

        auto old_view_thread = std::move(viewThread);

        lock.unlock();

        if (old_view_thread->getState() != ThreadStateStopped) {
            old_view_thread->join();
        }
    }

    void stopUart() {
        auto lock = mutex.asScopedLock();
        lock.lock();

        if (uart != nullptr && uart->isStarted()) {
            uart->stop();
            uart = nullptr;
        }
    }

    void onStart(lv_obj_t* parent, std::unique_ptr<Uart> newUart, bool enableUsbRelay, bool enableSdLog) {
        auto lock = mutex.asScopedLock();
        lock.lock();

        startLogic(std::move(newUart), enableUsbRelay, enableSdLog);
        startViews(parent);
    }

    void onStop() override {
        stopViews();
        stopLogic();
        stopUart();
    }
};
