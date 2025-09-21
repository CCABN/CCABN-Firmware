#include "wifi_manager.h"
#include "captive_portal.h"
#include "wifi_storage.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include <math.h>

static const char *TAG = "WiFiManager";

// Global state
static wifi_state_t current_state = WIFI_STATE_DISCONNECTED;
static char device_name[32];
static TimerHandle_t led_timer = NULL;
static TimerHandle_t button_timer = NULL;
static bool button_pressed = false;
static bool led_blink_state = false;
static uint8_t pulse_direction = 1;  // 1 for increasing, 0 for decreasing
static uint8_t pulse_brightness = 0;
static bool is_pulsing = false;
static wifi_state_t previous_led_state = WIFI_STATE_DISCONNECTED;
static TaskHandle_t mode_change_task_handle = NULL;

// Network interface tracking
static esp_netif_t* ap_netif = NULL;
static esp_netif_t* sta_netif = NULL;

// Forward declarations
static void button_isr_handler(void* arg);
static void button_hold_callback(TimerHandle_t xTimer);
static void mode_change_task(void* pvParameters);
static void led_pulse_callback(TimerHandle_t xTimer);
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

void wifi_manager_init(void) {
    // Generate device name from MAC address
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(device_name, sizeof(device_name), "CCABN_TRACKER_%02X%02X%02X", mac[3], mac[4], mac[5]);

    ESP_LOGI(TAG, "Device name: %s", device_name);

    // Initialize components
    status_led_init();
    button_init();

    // Initialize WiFi
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register WiFi event handler
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    // Try to load and connect to saved WiFi credentials
    wifi_credentials_t saved_credentials;
    if (wifi_storage_has_credentials() && wifi_storage_load_credentials(&saved_credentials)) {
        ESP_LOGI(TAG, "Found saved WiFi credentials, attempting to connect");
        wifi_manager_connect_sta(saved_credentials.ssid, saved_credentials.password);
    } else {
        ESP_LOGI(TAG, "No saved WiFi credentials found");
        current_state = WIFI_STATE_DISCONNECTED;
        status_led_start_pulse();  // Pulse LED to indicate no connection
    }
}

void wifi_manager_start_ap_mode(void) {
    ESP_LOGI(TAG, "Starting AP mode");
    current_state = WIFI_STATE_AP_MODE;

    // Stop WiFi if it's currently running
    esp_wifi_stop();

    // Destroy existing STA interface if it exists
    if (sta_netif != NULL) {
        esp_netif_destroy(sta_netif);
        sta_netif = NULL;
    }

    // Create AP network interface only if it doesn't exist
    if (ap_netif == NULL) {
        ap_netif = esp_netif_create_default_wifi_ap();
    }

    // Configure AP
    wifi_config_t wifi_config = {};
    strcpy((char*)wifi_config.ap.ssid, device_name);
    wifi_config.ap.ssid_len = strlen(device_name);
    wifi_config.ap.password[0] = '\0';  // Open network
    wifi_config.ap.channel = AP_CHANNEL;
    wifi_config.ap.max_connection = AP_MAX_CONNECTIONS;
    wifi_config.ap.authmode = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Start captive portal
    captive_portal_start();

    // Set LED to blink mode
    status_led_start_blink();

    ESP_LOGI(TAG, "AP mode started. SSID: %s", device_name);
}

void wifi_manager_stop_ap_mode(void) {
    ESP_LOGI(TAG, "Stopping AP mode");

    captive_portal_stop();
    esp_wifi_stop();

    // Destroy AP interface
    if (ap_netif != NULL) {
        esp_netif_destroy(ap_netif);
        ap_netif = NULL;
    }

    status_led_stop_effects();
    status_led_set_state(true);  // Solid on when connected
}

void wifi_manager_connect_sta(const char* ssid, const char* password) {
    ESP_LOGI(TAG, "Connecting to WiFi: %s", ssid);

    // Stop AP mode if currently active
    if (current_state == WIFI_STATE_AP_MODE) {
        wifi_manager_stop_ap_mode();
    }

    current_state = WIFI_STATE_CONNECTING;

    // Stop WiFi if it's currently running
    esp_wifi_stop();

    // Destroy existing AP interface if it exists
    if (ap_netif != NULL) {
        esp_netif_destroy(ap_netif);
        ap_netif = NULL;
    }

    // Create STA network interface only if it doesn't exist
    if (sta_netif == NULL) {
        sta_netif = esp_netif_create_default_wifi_sta();
    }

    // Configure STA
    wifi_config_t wifi_config = {};
    strcpy((char*)wifi_config.sta.ssid, ssid);
    strcpy((char*)wifi_config.sta.password, password);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());

    // Set LED to pulse while connecting
    status_led_start_pulse();
}

wifi_state_t wifi_manager_get_state(void) {
    return current_state;
}

char* wifi_manager_get_device_name(void) {
    return device_name;
}

// Status LED functions
void status_led_init(void) {
    // Configure LEDC timer
    ledc_timer_config_t ledc_timer = {};
    ledc_timer.speed_mode = LEDC_LOW_SPEED_MODE;
    ledc_timer.duty_resolution = LEDC_TIMER_8_BIT;
    ledc_timer.timer_num = LEDC_TIMER_0;
    ledc_timer.freq_hz = 1000;  // 1kHz
    ledc_timer.clk_cfg = LEDC_AUTO_CLK;
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Configure LEDC channel
    ledc_channel_config_t ledc_channel = {};
    ledc_channel.gpio_num = STATUS_LED_PIN;
    ledc_channel.speed_mode = LEDC_LOW_SPEED_MODE;
    ledc_channel.channel = LEDC_CHANNEL_0;
    ledc_channel.intr_type = LEDC_INTR_DISABLE;
    ledc_channel.timer_sel = LEDC_TIMER_0;
    ledc_channel.duty = 255;  // Start with LED on (full brightness)
    ledc_channel.hpoint = 0;
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    // Create timer for LED effects
    led_timer = xTimerCreate("led_timer", pdMS_TO_TICKS(50), pdTRUE, NULL, led_pulse_callback);

    // Start with LED on (normal operation)
    status_led_set_state(true);
}

void status_led_set_state(bool on) {
    is_pulsing = false;
    // Safe to use ESP_ERROR_CHECK here since it's not called from timer callback
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, on ? 255 : 0));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
}

void status_led_start_pulse(void) {
    is_pulsing = true;
    pulse_brightness = 50;  // Start at minimum brightness (not completely off)
    pulse_direction = 1;    // Start increasing
    if (led_timer) {
        xTimerChangePeriod(led_timer, pdMS_TO_TICKS(50), 0);  // 50ms for smooth pulsing
        xTimerStart(led_timer, 0);
    }
}

void status_led_start_blink(void) {
    is_pulsing = false;
    led_blink_state = false;
    if (led_timer) {
        xTimerChangePeriod(led_timer, pdMS_TO_TICKS(500), 0);   // 0.5 second blink
        xTimerStart(led_timer, 0);
    }
}

void status_led_stop_effects(void) {
    if (led_timer) {
        xTimerStop(led_timer, 0);
    }
}

// Button functions
void button_init(void) {
    ESP_LOGI(TAG, "Initializing button on GPIO %d", BUTTON_PIN);

    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_ANYEDGE;  // Trigger on both edges to detect press and release
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << BUTTON_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;  // Pull down when not connected
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO %d: %s", BUTTON_PIN, esp_err_to_name(ret));
        return;
    }

    ret = gpio_install_isr_service(0);
    if (ret != ESP_OK) {
        if (ret == ESP_ERR_INVALID_STATE) {
            ESP_LOGI(TAG, "GPIO ISR service already installed");
        } else {
            ESP_LOGE(TAG, "Failed to install GPIO ISR service: %s", esp_err_to_name(ret));
            return;
        }
    } else {
        ESP_LOGI(TAG, "GPIO ISR service installed successfully");
    }

    ret = gpio_isr_handler_add((gpio_num_t)BUTTON_PIN, button_isr_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add ISR handler for GPIO %d: %s", BUTTON_PIN, esp_err_to_name(ret));
        return;
    }

    // Create timer for button hold detection
    button_timer = xTimerCreate("button_timer", pdMS_TO_TICKS(BUTTON_HOLD_TIME_MS), pdFALSE, NULL, button_hold_callback);
    if (button_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create button timer");
        return;
    }

    ESP_LOGI(TAG, "Button initialized successfully on GPIO %d", BUTTON_PIN);
    ESP_LOGI(TAG, "Current button state: %d", gpio_get_level((gpio_num_t)BUTTON_PIN));
}

// Callback functions
static void led_pulse_callback(TimerHandle_t xTimer) {
    if (is_pulsing) {
        // True PWM pulsing - brightness goes from 50 to 255 and back
        if (pulse_direction) {
            pulse_brightness += 5;
            if (pulse_brightness >= 255) {
                pulse_direction = 0;  // Start decreasing
            }
        } else {
            pulse_brightness -= 5;
            if (pulse_brightness <= 50) {
                pulse_direction = 1;  // Start increasing
            }
        }
        // Don't use ESP_ERROR_CHECK in timer callbacks - can cause stack overflow
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, pulse_brightness);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    } else {
        // Regular blinking for AP mode
        led_blink_state = !led_blink_state;
        // Don't use ESP_ERROR_CHECK in timer callbacks - can cause stack overflow
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, led_blink_state ? 255 : 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    }
}

static void button_isr_handler(void* arg) {
    // Don't log from ISR - can cause crashes
    int gpio_level = gpio_get_level((gpio_num_t)BUTTON_PIN);
    if (gpio_level == 1 && !button_pressed) {
        // Rising edge - button pressed
        button_pressed = true;
        previous_led_state = current_state;  // Remember current state
        status_led_set_state(false);  // Turn off LED immediately
        BaseType_t higher_priority_task_woken = pdFALSE;
        xTimerStartFromISR(button_timer, &higher_priority_task_woken);
        portYIELD_FROM_ISR(higher_priority_task_woken);
    }
}

static void mode_change_task(void* pvParameters) {
    int action = (int)pvParameters;

    if (action == 1) {
        ESP_LOGI(TAG, "Starting AP mode from task");
        wifi_manager_start_ap_mode();
    } else if (action == 0) {
        ESP_LOGI(TAG, "Stopping AP mode from task");
        wifi_manager_stop_ap_mode();
        current_state = WIFI_STATE_DISCONNECTED;
        status_led_start_pulse();
    } else if (action == 2) {
        ESP_LOGI(TAG, "Restoring previous LED state");
        switch (previous_led_state) {
            case WIFI_STATE_CONNECTED:
                status_led_set_state(true);
                break;
            case WIFI_STATE_DISCONNECTED:
            case WIFI_STATE_CONNECTING:
                status_led_start_pulse();
                break;
            case WIFI_STATE_AP_MODE:
                status_led_start_blink();
                break;
        }
    }

    // Delete this task
    mode_change_task_handle = NULL;
    vTaskDelete(NULL);
}

static void button_hold_callback(TimerHandle_t xTimer) {
    // Minimal timer callback - just create task, no other operations
    if (button_pressed && mode_change_task_handle == NULL) {
        if (gpio_get_level((gpio_num_t)BUTTON_PIN) == 1) {
            // Button still held - start mode change task
            int action = (current_state != WIFI_STATE_AP_MODE) ? 1 : 0;
            xTaskCreate(mode_change_task, "mode_change", 4096, (void*)action, 5, &mode_change_task_handle);
        } else {
            // Button released - start restore task
            xTaskCreate(mode_change_task, "restore", 4096, (void*)2, 5, &mode_change_task_handle);
        }
    }
    button_pressed = false;
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "Connected to WiFi");
                current_state = WIFI_STATE_CONNECTED;
                status_led_stop_effects();
                status_led_set_state(true);  // Solid on
                break;

            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGI(TAG, "Disconnected from WiFi");
                current_state = WIFI_STATE_DISCONNECTED;
                status_led_start_pulse();
                break;

            case WIFI_EVENT_AP_STACONNECTED: {
                wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
                ESP_LOGI(TAG, "Station connected: " MACSTR, MAC2STR(event->mac));
                break;
            }

            case WIFI_EVENT_AP_STADISCONNECTED: {
                wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
                ESP_LOGI(TAG, "Station disconnected: " MACSTR, MAC2STR(event->mac));
                break;
            }
        }
    }
}