#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

// ReSharper disable once CppUnusedIncludeDirective
#include "esp_wifi.h"
#include "esp_http_server.h"

// Pin definitions - easily changeable
#define BUTTON_PIN          2     // D1 pin on Xiao ESP32-S3
#define STATUS_LED_PIN      21    // Built-in LED on Xiao ESP32-S3

// WiFi AP settings
#define AP_CHANNEL          1
#define AP_MAX_CONNECTIONS  4
#define BUTTON_HOLD_TIME_MS 3000

// Wi-Fi manager states
typedef enum {
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_AP_MODE
} wifi_state_t;

// Function declarations
void wifi_manager_init();
void wifi_manager_start_ap_mode();
void wifi_manager_stop_ap_mode();
void wifi_manager_connect_sta(const char* ssid, const char* password);
wifi_state_t wifi_manager_get_state();
char* wifi_manager_get_device_name();

// Status LED control
void status_led_init();
void status_led_set_state(bool on);
void status_led_start_pulse();
void status_led_start_blink();
void status_led_stop_effects();

// Button handling
void button_init();

#endif // WIFI_MANAGER_H