#pragma once

#include "View.h"

#include <string>
#include <vector>
#include <lvgl.h>
#include <Str.h>
#include <functional>
#include <memory>

#include <tt_app_alertdialog.h>
#include <tt_hal_uart.h>
#include <tt_lvgl.h>
#include <TactilityCpp/LvglLock.h>
#include <TactilityCpp/Uart.h>
#include <TactilityCpp/Preferences.h>

class ConnectView final : public View {

public:

    typedef std::function<void(std::unique_ptr<Uart>, bool, bool)> OnConnectedFunction;
    std::vector<Str> uartNames;
    Preferences preferences = Preferences("UartRelay");
    LvglLock lvglLock;

private:

    OnConnectedFunction onConnected;
    lv_obj_t* busDropdown = nullptr;
    lv_obj_t* baudDropdown = nullptr;
    lv_obj_t* customBaudTextarea = nullptr;
    lv_obj_t* usbRelaySwitch = nullptr;
    lv_obj_t* sdLogSwitch = nullptr;
    
    // Track switch states manually to avoid LVGL version issues
    bool usbRelaySwitchState = false;
    bool sdLogSwitchState = false;
    
    const char* baudRates[9] = {"9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600", "Custom"};
    int customBaudRate = 115200;

    Str join(const std::vector<Str>& list) {
        Str output;
        for (int i = list.size() - 1; i >= 0; i--) {
            output.append(list[i].c_str());
            if (i < list.size() - 1) {
                output.append(",");
            }
        }
        return output;
    }

    int32_t getSpeedInput() const {
        uint16_t selected = lv_dropdown_get_selected(baudDropdown);
        
        if (selected == 8) { // Custom selected
            auto* speed_text = lv_textarea_get_text(customBaudTextarea);
            return atoi(speed_text);
        } else if (selected < 8) {
            return atoi(baudRates[selected]);
        }
        
        return 115200; // Default fallback
    }

    static void onBaudDropdownChanged(lv_event_t* event) {
        auto* view = static_cast<ConnectView*>(lv_event_get_user_data(event));
        view->handleBaudDropdownChange();
    }

    void handleBaudDropdownChange() {
        uint16_t selected = lv_dropdown_get_selected(baudDropdown);
        
        if (selected == 8) { // Custom selected
            lv_obj_remove_flag(customBaudTextarea, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(customBaudTextarea, LV_OBJ_FLAG_HIDDEN);
        }
    }

    static void onUsbRelaySwitchChanged(lv_event_t* event) {
        auto* view = static_cast<ConnectView*>(lv_event_get_user_data(event));
        view->handleUsbRelayChange();
    }

    static void onSdLogSwitchChanged(lv_event_t* event) {
        auto* view = static_cast<ConnectView*>(lv_event_get_user_data(event));
        view->handleSdLogChange();
    }

    void handleUsbRelayChange() {
        // Toggle our tracked state
        usbRelaySwitchState = !usbRelaySwitchState;
        
        // If USB relay is ON, disable SD Log
        if (usbRelaySwitchState) {
            sdLogSwitchState = false;
            lv_obj_clear_state(sdLogSwitch, LV_STATE_CHECKED);
            lv_obj_add_state(sdLogSwitch, LV_STATE_DISABLED);
        } else {
            lv_obj_clear_state(sdLogSwitch, LV_STATE_DISABLED);
        }
    }

    void handleSdLogChange() {
        // Only toggle if not disabled
        if (!usbRelaySwitchState) {
            sdLogSwitchState = !sdLogSwitchState;
        }
    }

    void onConnect() {
        auto lock = lvglLock.asScopedLock();
        if (!lock.lock(TT_LVGL_DEFAULT_LOCK_TIME)) {
            return;
        }

        const char* alert_dialog_labels[] = { "OK" };

        auto selected_uart_index = lv_dropdown_get_selected(busDropdown);
        if (selected_uart_index >= uartNames.size()) {
            tt_app_alertdialog_start("Error", "No UART selected", alert_dialog_labels, 1);
            return;
        }

        auto uart = Uart::open(selected_uart_index);
        if (uart == nullptr) {
            tt_app_alertdialog_start("Error", "Failed to connect to UART", alert_dialog_labels, 1);
            return;
        }

        int speed = getSpeedInput();
        if (speed <= 0) {
            tt_app_alertdialog_start("Error", "Invalid speed", alert_dialog_labels, 1);
            return;
        }

        if (!uart->start()) {
            tt_app_alertdialog_start("Error", "Failed to initialize", alert_dialog_labels, 1);
            return;
        }

        if (!uart->setBaudRate(speed)) {
            uart->stop();
            tt_app_alertdialog_start("Error", "Failed to set baud rate", alert_dialog_labels, 1);
            return;
        }

        // Get USB Relay and SD Log settings from our tracked state
        bool usbRelayEnabled = usbRelaySwitchState;
        bool sdLogEnabled = sdLogSwitchState;

        onConnected(std::move(uart), usbRelayEnabled, sdLogEnabled);
    }

    static void onConnectCallback(lv_event_t* event) {
        auto* view = static_cast<ConnectView*>(lv_event_get_user_data(event));
        view->onConnect();
    }

    static lv_obj_t* createRowWrapper(lv_obj_t* parent) {
        auto* wrapper = lv_obj_create(parent);
        lv_obj_set_size(wrapper, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_border_width(wrapper, 0, LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(wrapper, 0, LV_STATE_DEFAULT);
        return wrapper;
    }

public:

    explicit ConnectView(OnConnectedFunction onConnected) : onConnected(std::move(onConnected)) {}

    void onStart(lv_obj_t* parent) {
        uartNames = Uart::getNames();

        auto* wrapper = lv_obj_create(parent);
        lv_obj_set_flex_flow(wrapper, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_size(wrapper, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_border_width(wrapper, 0, LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(wrapper, 0, LV_STATE_DEFAULT);

        // Bus selection
        auto* bus_wrapper = createRowWrapper(wrapper);

        busDropdown = lv_dropdown_create(bus_wrapper);
        auto bus_options = join(uartNames);
        lv_dropdown_set_options(busDropdown, bus_options.c_str());
        lv_obj_align(busDropdown, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_set_width(busDropdown, LV_PCT(50));

        int32_t bus_index = 0;
        preferences.optInt32("bus", bus_index);
        if (bus_index < uartNames.size()) {
            lv_dropdown_set_selected(busDropdown, bus_index);
        }

        auto* bus_label = lv_label_create(bus_wrapper);
        lv_obj_align(bus_label, LV_ALIGN_LEFT_MID, 0, 0);
        lv_label_set_text(bus_label, "Bus");

        // Baud rate dropdown
        auto* baud_wrapper = createRowWrapper(wrapper);

        baudDropdown = lv_dropdown_create(baud_wrapper);
        lv_dropdown_set_options(baudDropdown, "9600\n19200\n38400\n57600\n115200\n230400\n460800\n921600\nCustom");
        lv_obj_set_width(baudDropdown, LV_PCT(50));
        lv_obj_align(baudDropdown, LV_ALIGN_TOP_RIGHT, 0, 0);
        lv_obj_add_event_cb(baudDropdown, onBaudDropdownChanged, LV_EVENT_VALUE_CHANGED, this);

        // Set default to 115200 (index 4)
        int32_t saved_baud_index = 4;
        preferences.optInt32("baud_index", saved_baud_index);
        lv_dropdown_set_selected(baudDropdown, saved_baud_index);

        auto* baud_rate_label = lv_label_create(baud_wrapper);
        lv_obj_align(baud_rate_label, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_label_set_text(baud_rate_label, "Baud");

        // Custom baud rate input (hidden by default)
        auto* custom_baud_wrapper = createRowWrapper(wrapper);
        
        customBaudTextarea = lv_textarea_create(custom_baud_wrapper);
        lv_textarea_set_text(customBaudTextarea, "115200");
        lv_textarea_set_one_line(customBaudTextarea, true);
        lv_obj_set_width(customBaudTextarea, LV_PCT(50));
        lv_obj_align(customBaudTextarea, LV_ALIGN_TOP_RIGHT, 0, 0);
        lv_obj_add_flag(customBaudTextarea, LV_OBJ_FLAG_HIDDEN); // Hidden by default

        auto* custom_baud_label = lv_label_create(custom_baud_wrapper);
        lv_obj_align(custom_baud_label, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_label_set_text(custom_baud_label, "Custom Baud");

        // USB Relay toggle
        auto* usb_wrapper = createRowWrapper(wrapper);
        
        auto* usb_label = lv_label_create(usb_wrapper);
        lv_label_set_text(usb_label, "USB Relay:");
        lv_obj_align(usb_label, LV_ALIGN_LEFT_MID, 0, 0);
        
        usbRelaySwitch = lv_switch_create(usb_wrapper);
        lv_obj_align(usbRelaySwitch, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_add_event_cb(usbRelaySwitch, onUsbRelaySwitchChanged, LV_EVENT_VALUE_CHANGED, this);

        // SD Log toggle
        auto* sd_wrapper = createRowWrapper(wrapper);
        
        auto* sd_label = lv_label_create(sd_wrapper);
        lv_label_set_text(sd_label, "SD Log:");
        lv_obj_align(sd_label, LV_ALIGN_LEFT_MID, 0, 0);
        
        sdLogSwitch = lv_switch_create(sd_wrapper);
        lv_obj_align(sdLogSwitch, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_add_event_cb(sdLogSwitch, onSdLogSwitchChanged, LV_EVENT_VALUE_CHANGED, this);

        // Connect button
        auto* connect_wrapper = createRowWrapper(wrapper);

        auto* connect_button = lv_button_create(connect_wrapper);
        lv_obj_align(connect_button, LV_ALIGN_CENTER, 0, 0);
        lv_obj_add_event_cb(connect_button, onConnectCallback, LV_EVENT_SHORT_CLICKED, this);
        auto* connect_label = lv_label_create(connect_button);
        lv_label_set_text(connect_label, "Connect");
    }

    void onStop() override {
        // Save baud rate selection
        auto baud_index = static_cast<int32_t>(lv_dropdown_get_selected(baudDropdown));
        preferences.putInt32("baud_index", baud_index);

        // Save bus selection
        auto bus_index = static_cast<int32_t>(lv_dropdown_get_selected(busDropdown));
        preferences.putInt32("bus", bus_index);
    }
};
