#ifndef WIFI_STATE_MACHINE_H
#define WIFI_STATE_MACHINE_H

#include "esp_event.h"

// Wi-Fi State Machine States
typedef enum {
    WIFI_SM_DISCONNECTED,    // Device powered on, no Wi-Fi connection
    WIFI_SM_CONNECTING,      // Attempting to connect to saved WiFi credentials
    WIFI_SM_CONNECTED,       // Successfully connected to Wi-Fi network
    WIFI_SM_AP_MODE,         // Access Point mode for configuration
    WIFI_SM_TRANSITIONING   // Temporary state during transitions
} wifi_sm_state_t;

// Wi-Fi State Machine Events
typedef enum {
    WIFI_SM_EVENT_INIT,              // System initialization
    WIFI_SM_EVENT_BUTTON_HOLD,       // Button held for 3+ seconds
    WIFI_SM_EVENT_BUTTON_RELEASE,    // Button released, exit AP mode
    WIFI_SM_EVENT_CREDENTIALS_SAVED, // New Wi-Fi credentials saved
    WIFI_SM_EVENT_CONNECT_SUCCESS,   // Wi-Fi connection established
    WIFI_SM_EVENT_CONNECT_FAILED,    // Wi-Fi connection failed
    WIFI_SM_EVENT_DISCONNECT         // Wi-Fi connection lost
} wifi_sm_event_t;

// State transition function prototype
typedef wifi_sm_state_t (*state_transition_fn_t)(wifi_sm_event_t event, void* data);

// State machine structure
typedef struct {
    wifi_sm_state_t current_state;
    wifi_sm_state_t previous_state;
    bool transition_in_progress;
    char device_name[32];
} wifi_state_machine_t;

// Function declarations
void wifi_state_machine_init();
wifi_sm_state_t wifi_state_machine_get_state();
bool wifi_state_machine_send_event(wifi_sm_event_t event, void* data);
const char* wifi_state_machine_state_name(wifi_sm_state_t state);
const char* wifi_state_machine_event_name(wifi_sm_event_t event);

// State handler functions
wifi_sm_state_t state_disconnected_handler(wifi_sm_event_t event, void* data);
wifi_sm_state_t state_connecting_handler(wifi_sm_event_t event, void* data);
wifi_sm_state_t state_connected_handler(wifi_sm_event_t event, void* data);
wifi_sm_state_t state_ap_mode_handler(wifi_sm_event_t event, void* data);

// State entry/exit functions
void state_disconnected_enter();
void state_disconnected_exit();
void state_connecting_enter();
void state_connecting_exit();
void state_connected_enter();
void state_connected_exit();
void state_ap_mode_enter();
void state_ap_mode_exit();

#endif // WIFI_STATE_MACHINE_H