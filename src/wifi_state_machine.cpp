#include "wifi_state_machine.h"
#include "network_scanner.h"
#include "captive_portal.h"
#include "status_led.h"
#include "wifi_storage.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include <string.h>

static const char *TAG = "WiFiStateMachine";

// State machine instance
// Use C++ style value initialization {} instead of C-style {0}
static wifi_state_machine_t state_machine = {};

// Network interfaces
static esp_netif_t* sta_netif = nullptr;
static esp_netif_t* ap_netif = nullptr;

// Forward declarations
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void credentials_saved_callback(const char* ssid, const char* password);
static bool transition_to_state(wifi_sm_state_t new_state);
static void cleanup_current_state(void);

void wifi_state_machine_init(void) {
    ESP_LOGI(TAG, "Initializing WiFi state machine");

    // Generate device name from MAC address
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(state_machine.device_name, sizeof(state_machine.device_name),
             "CCABN_TRACKER_%02X%02X%02X", mac[3], mac[4], mac[5]);

    // Initialize state machine
    state_machine.current_state = WIFI_SM_DISCONNECTED;
    state_machine.previous_state = WIFI_SM_DISCONNECTED;
    state_machine.transition_in_progress = false;

    // Initialize WiFi subsystem
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register WiFi event handler
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &wifi_event_handler,
        NULL,
        NULL));

    // Initialize modules
    network_scanner_init();

    captive_portal_config_t portal_config = {
        .device_name = state_machine.device_name,
        .http_port = 80,
        .dns_port = 53,
        .ap_ip = "192.168.4.1"
    };
    captive_portal_init(&portal_config);
    captive_portal_set_credentials_callback(credentials_saved_callback);

    status_led_config_t led_config = {
        .gpio_pin = 21,        // STATUS_LED_PIN from original code
        .pwm_frequency = 1000,
        .ledc_channel = 0,
        .ledc_timer = 0
    };
    status_led_init(&led_config);

    ESP_LOGI(TAG, "WiFi state machine initialized with device name: %s", state_machine.device_name);

    // Start in disconnected state
    wifi_state_machine_send_event(WIFI_SM_EVENT_INIT, nullptr);
}

wifi_sm_state_t wifi_state_machine_get_state(void) {
    return state_machine.current_state;
}

bool wifi_state_machine_send_event(wifi_sm_event_t event, void* data) {
    if (state_machine.transition_in_progress) {
        ESP_LOGW(TAG, "Transition in progress, ignoring event %s",
                 wifi_state_machine_event_name(event));
        return false;
    }

    ESP_LOGI(TAG, "Event: %s in state: %s",
             wifi_state_machine_event_name(event),
             wifi_state_machine_state_name(state_machine.current_state));

    wifi_sm_state_t new_state = state_machine.current_state;

    // Handle events based on current state
    switch (state_machine.current_state) {
        case WIFI_SM_DISCONNECTED:
            new_state = state_disconnected_handler(event, data);
            break;
        case WIFI_SM_CONNECTING:
            new_state = state_connecting_handler(event, data);
            break;
        case WIFI_SM_CONNECTED:
            new_state = state_connected_handler(event, data);
            break;
        case WIFI_SM_AP_MODE:
            new_state = state_ap_mode_handler(event, data);
            break;
        case WIFI_SM_TRANSITIONING:
            ESP_LOGW(TAG, "Event received during transition, ignoring");
            return false;
        default:
            ESP_LOGE(TAG, "Unknown state: %d", state_machine.current_state);
            return false;
    }

    // Transition to new state if changed
    if (new_state != state_machine.current_state) {
        return transition_to_state(new_state);
    }

    return true;
}

// State handler implementations
wifi_sm_state_t state_disconnected_handler(wifi_sm_event_t event, void* data) {
    switch (event) {
        case WIFI_SM_EVENT_INIT:
            // Check for saved credentials and try to connect
            if (wifi_storage_has_credentials()) {
                return WIFI_SM_CONNECTING;
            }
            // Stay disconnected if no credentials
            return WIFI_SM_DISCONNECTED;

        case WIFI_SM_EVENT_BUTTON_HOLD:
            return WIFI_SM_AP_MODE;

        case WIFI_SM_EVENT_CREDENTIALS_SAVED:
            return WIFI_SM_CONNECTING;

        default:
            return WIFI_SM_DISCONNECTED;
    }
}

wifi_sm_state_t state_connecting_handler(wifi_sm_event_t event, void* data) {
    switch (event) {
        case WIFI_SM_EVENT_CONNECT_SUCCESS:
            return WIFI_SM_CONNECTED;

        case WIFI_SM_EVENT_CONNECT_FAILED:
            return WIFI_SM_DISCONNECTED;

        case WIFI_SM_EVENT_BUTTON_HOLD:
            return WIFI_SM_AP_MODE;

        default:
            return WIFI_SM_CONNECTING;
    }
}

wifi_sm_state_t state_connected_handler(wifi_sm_event_t event, void* data) {
    switch (event) {
        case WIFI_SM_EVENT_DISCONNECT:
            return WIFI_SM_DISCONNECTED;

        case WIFI_SM_EVENT_BUTTON_HOLD:
            return WIFI_SM_AP_MODE;

        default:
            return WIFI_SM_CONNECTED;
    }
}

wifi_sm_state_t state_ap_mode_handler(wifi_sm_event_t event, void* data) {
    switch (event) {
        case WIFI_SM_EVENT_BUTTON_RELEASE:
            // Return to appropriate state based on credentials
            if (wifi_storage_has_credentials()) {
                return WIFI_SM_CONNECTING;
            } else {
                return WIFI_SM_DISCONNECTED;
            }

        case WIFI_SM_EVENT_CREDENTIALS_SAVED:
            // Stay in AP mode until user exits
            return WIFI_SM_AP_MODE;

        default:
            return WIFI_SM_AP_MODE;
    }
}

// State entry functions
void state_disconnected_enter(void) {
    ESP_LOGI(TAG, "Entering DISCONNECTED state");

    // Stop any scanning
    network_scanner_stop_continuous();

    // Set LED to pulse
    status_led_set_pattern(LED_PATTERN_PULSE);

    ESP_LOGI(TAG, "DISCONNECTED state active");
}

void state_connecting_enter(void) {
    ESP_LOGI(TAG, "Entering CONNECTING state");

    // Load credentials and attempt connection
    wifi_credentials_t credentials;
    if (!wifi_storage_load_credentials(&credentials)) {
        ESP_LOGE(TAG, "Failed to load credentials in CONNECTING state");
        wifi_state_machine_send_event(WIFI_SM_EVENT_CONNECT_FAILED, nullptr);
        return;
    }

    // Create STA interface if needed
    if (sta_netif == nullptr) {
        sta_netif = esp_netif_create_default_wifi_sta();
    }

    // Configure and start Wi-Fi connection
    // Use C++ style value initialization {} instead of C-style {0}
    wifi_config_t wifi_config = {};
    strncpy(reinterpret_cast<char *>(wifi_config.sta.ssid), credentials.ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy(reinterpret_cast<char *>(wifi_config.sta.password), credentials.password, sizeof(wifi_config.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());

    // Set LED to pulse
    status_led_set_pattern(LED_PATTERN_PULSE);

    ESP_LOGI(TAG, "CONNECTING state active, attempting connection to: %s", credentials.ssid);
}

void state_connected_enter(void) {
    ESP_LOGI(TAG, "Entering CONNECTED state");

    // Set LED to solid
    status_led_set_pattern(LED_PATTERN_SOLID);

    ESP_LOGI(TAG, "CONNECTED state active");
}

void state_ap_mode_enter(void) {
    ESP_LOGI(TAG, "Entering AP_MODE state");

    // Stop Wi-Fi and clean up STA interface
    esp_wifi_stop();

    if (sta_netif != nullptr) {
        esp_netif_destroy(sta_netif);
        sta_netif = nullptr;
    }

    // Create both interfaces for APSTA mode
    if (sta_netif == nullptr) {
        sta_netif = esp_netif_create_default_wifi_sta();
    }
    if (ap_netif == nullptr) {
        ap_netif = esp_netif_create_default_wifi_ap();
    }

    // Configure AP
    // Use C++ style value initialization {} instead of C-style {0}
    wifi_config_t ap_config = {};
    strncpy(reinterpret_cast<char *>(ap_config.ap.ssid), state_machine.device_name, sizeof(ap_config.ap.ssid) - 1);
    ap_config.ap.ssid_len = strlen(state_machine.device_name);
    ap_config.ap.password[0] = '\0';  // Open network
    ap_config.ap.channel = 1;
    ap_config.ap.max_connection = 4;
    ap_config.ap.authmode = WIFI_AUTH_OPEN;

    // Set to APSTA mode and start
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Start network scanner for continuous scanning
    network_scanner_start_continuous(4000);  // Scan every 4 seconds

    // Start captive portal
    captive_portal_start();

    // Set LED to blink
    status_led_set_pattern(LED_PATTERN_BLINK);

    ESP_LOGI(TAG, "AP_MODE state active, SSID: %s", state_machine.device_name);
}

// State exit functions
void state_disconnected_exit(void) {
    ESP_LOGD(TAG, "Exiting DISCONNECTED state");
}

void state_connecting_exit(void) {
    ESP_LOGD(TAG, "Exiting CONNECTING state");
}

void state_connected_exit(void) {
    ESP_LOGD(TAG, "Exiting CONNECTED state");
    esp_wifi_disconnect();
}

void state_ap_mode_exit(void) {
    ESP_LOGI(TAG, "Exiting AP_MODE state");

    // Stop captive portal
    captive_portal_stop();

    // Stop network scanner
    network_scanner_stop_continuous();

    // Stop Wi-Fi
    esp_wifi_stop();

    // Clean up AP interface
    if (ap_netif != nullptr) {
        esp_netif_destroy(ap_netif);
        ap_netif = nullptr;
    }
}

// Helper functions
const char* wifi_state_machine_state_name(wifi_sm_state_t state) {
    switch (state) {
        case WIFI_SM_DISCONNECTED: return "DISCONNECTED";
        case WIFI_SM_CONNECTING: return "CONNECTING";
        case WIFI_SM_CONNECTED: return "CONNECTED";
        case WIFI_SM_AP_MODE: return "AP_MODE";
        case WIFI_SM_TRANSITIONING: return "TRANSITIONING";
        default: return "UNKNOWN";
    }
}

const char* wifi_state_machine_event_name(wifi_sm_event_t event) {
    switch (event) {
        case WIFI_SM_EVENT_INIT: return "INIT";
        case WIFI_SM_EVENT_BUTTON_HOLD: return "BUTTON_HOLD";
        case WIFI_SM_EVENT_BUTTON_RELEASE: return "BUTTON_RELEASE";
        case WIFI_SM_EVENT_CREDENTIALS_SAVED: return "CREDENTIALS_SAVED";
        case WIFI_SM_EVENT_CONNECT_SUCCESS: return "CONNECT_SUCCESS";
        case WIFI_SM_EVENT_CONNECT_FAILED: return "CONNECT_FAILED";
        case WIFI_SM_EVENT_DISCONNECT: return "DISCONNECT";
        default: return "UNKNOWN";
    }
}

// Private functions
static bool transition_to_state(wifi_sm_state_t new_state) {
    if (state_machine.transition_in_progress) {
        ESP_LOGW(TAG, "Transition already in progress");
        return false;
    }

    ESP_LOGI(TAG, "State transition: %s -> %s",
             wifi_state_machine_state_name(state_machine.current_state),
             wifi_state_machine_state_name(new_state));

    state_machine.transition_in_progress = true;
    state_machine.previous_state = state_machine.current_state;

    // Exit current state
    cleanup_current_state();

    // Update state
    state_machine.current_state = new_state;

    // Enter new state
    switch (new_state) {
        case WIFI_SM_DISCONNECTED:
            state_disconnected_enter();
            break;
        case WIFI_SM_CONNECTING:
            state_connecting_enter();
            break;
        case WIFI_SM_CONNECTED:
            state_connected_enter();
            break;
        case WIFI_SM_AP_MODE:
            state_ap_mode_enter();
            break;
        default:
            ESP_LOGE(TAG, "Unknown state in transition: %d", new_state);
            state_machine.transition_in_progress = false;
            return false;
    }

    state_machine.transition_in_progress = false;
    return true;
}

static void cleanup_current_state(void) {
    switch (state_machine.current_state) {
        case WIFI_SM_DISCONNECTED:
            state_disconnected_exit();
            break;
        case WIFI_SM_CONNECTING:
            state_connecting_exit();
            break;
        case WIFI_SM_CONNECTED:
            state_connected_exit();
            break;
        case WIFI_SM_AP_MODE:
            state_ap_mode_exit();
            break;
        default:
            break;
    }
}

// WiFi event handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "WiFi connected");
                wifi_state_machine_send_event(WIFI_SM_EVENT_CONNECT_SUCCESS, nullptr);
                break;

            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGI(TAG, "WiFi disconnected");
                if (state_machine.current_state == WIFI_SM_CONNECTED) {
                    wifi_state_machine_send_event(WIFI_SM_EVENT_DISCONNECT, nullptr);
                } else if (state_machine.current_state == WIFI_SM_CONNECTING) {
                    wifi_state_machine_send_event(WIFI_SM_EVENT_CONNECT_FAILED, nullptr);
                }
                break;

            case WIFI_EVENT_AP_STACONNECTED: {
                wifi_event_ap_staconnected_t* event = static_cast<wifi_event_ap_staconnected_t *>(event_data);
                ESP_LOGI(TAG, "Station connected to AP: " MACSTR, MAC2STR(event->mac));
                break;
            }

            case WIFI_EVENT_AP_STADISCONNECTED: {
                wifi_event_ap_stadisconnected_t* event = static_cast<wifi_event_ap_stadisconnected_t *>(event_data);
                ESP_LOGI(TAG, "Station disconnected from AP: " MACSTR, MAC2STR(event->mac));
                break;
            }

            default:
                break;
        }
    }
}

// Captive portal callback
static void credentials_saved_callback(const char* ssid, const char* password) {
    ESP_LOGI(TAG, "Credentials saved via captive portal: %s", ssid);
    wifi_state_machine_send_event(WIFI_SM_EVENT_CREDENTIALS_SAVED, nullptr);
}
