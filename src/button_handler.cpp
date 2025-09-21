#include "button_handler.h"
#include "wifi_state_machine.h"
#include "status_led.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

static const char *TAG = "ButtonHandler";

// Configuration and state
static button_config_t button_config = {};
static bool is_initialized = false;
static bool button_pressed = false;
static bool hold_processed = false;

// Timers
static TimerHandle_t hold_timer = nullptr;

// Forward declarations
static void IRAM_ATTR button_isr_handler(void* arg);
static void button_hold_timer_callback(TimerHandle_t timer);

bool button_handler_init(const button_config_t* config) {
    if (is_initialized) {
        ESP_LOGW(TAG, "Button handler already initialized");
        return false;
    }

    if (!config) {
        ESP_LOGE(TAG, "Invalid configuration");
        return false;
    }

    ESP_LOGI(TAG, "Initializing button handler on GPIO %d", config->gpio_pin);

    // Copy configuration with defaults
    button_config = *config;
    if (button_config.hold_time_ms == 0) {
        button_config.hold_time_ms = 3000;
    }
    if (button_config.debounce_ms == 0) {
        button_config.debounce_ms = 50;
    }

    // Configure GPIO
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << button_config.gpio_pin);

    if (button_config.active_high) {
        io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    } else {
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    }

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO: %s", esp_err_to_name(ret));
        return false;
    }

    // Install ISR service if not already installed
    ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to install ISR service: %s", esp_err_to_name(ret));
        return false;
    }

    // Add ISR handler
    ret = gpio_isr_handler_add((gpio_num_t)button_config.gpio_pin, button_isr_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add ISR handler: %s", esp_err_to_name(ret));
        return false;
    }

    // Create hold timer
    hold_timer = xTimerCreate(
        "button_hold",
        pdMS_TO_TICKS(button_config.hold_time_ms),
        pdFALSE,  // One-shot timer
        NULL,
        button_hold_timer_callback
    );

    if (!hold_timer) {
        ESP_LOGE(TAG, "Failed to create hold timer");
        gpio_isr_handler_remove((gpio_num_t)button_config.gpio_pin);
        return false;
    }

    // Initialize state
    button_pressed = false;
    hold_processed = false;

    is_initialized = true;
    ESP_LOGI(TAG, "Button handler initialized successfully");

    return true;
}

void button_handler_deinit(void) {
    if (!is_initialized) {
        return;
    }

    ESP_LOGI(TAG, "Deinitializing button handler");

    // Remove ISR handler
    gpio_isr_handler_remove((gpio_num_t)button_config.gpio_pin);

    // Delete timer
    if (hold_timer) {
        xTimerStop(hold_timer, 0);
        xTimerDelete(hold_timer, 0);
        hold_timer = NULL;
    }

    is_initialized = false;
    ESP_LOGI(TAG, "Button handler deinitialized");
}

bool button_handler_is_initialized(void) {
    return is_initialized;
}

bool button_handler_is_pressed(void) {
    if (!is_initialized) {
        return false;
    }

    const int level = gpio_get_level((gpio_num_t)button_config.gpio_pin);
    return button_config.active_high ? (level == 1) : (level == 0);
}

// Private functions
static void IRAM_ATTR button_isr_handler(void* arg) {
    BaseType_t higher_priority_task_woken = pdFALSE;
    bool current_pressed = button_handler_is_pressed();

    if (current_pressed && !button_pressed) {
        // Button just pressed
        button_pressed = true;
        hold_processed = false;

        // Notify LED that button is pressed
        status_led_button_pressed();

        // Start hold timer
        xTimerStartFromISR(hold_timer, &higher_priority_task_woken);

    } else if (!current_pressed && button_pressed) {
        // Button just released
        button_pressed = false;

        // Stop hold timer
        xTimerStopFromISR(hold_timer, &higher_priority_task_woken);

        // Notify LED that button is released
        status_led_button_released();

        // Send release event to state machine if hold wasn't processed
        if (!hold_processed) {
            // Button was released before hold time - no state change
        } else {
            // Button was released after hold time - send release event
            // Note: We can't call the state machine from ISR, so we'll need
            // a task for this. For now, the state machine will handle this
            // through the LED system integration.
        }
    }

    portYIELD_FROM_ISR(higher_priority_task_woken);
}

static void button_hold_timer_callback(TimerHandle_t timer) {
    if (!button_pressed || hold_processed) {
        return; // Button released or already processed
    }

    // Verify button is still pressed (debounce check)
    if (!button_handler_is_pressed()) {
        return;
    }

    ESP_LOGI(TAG, "Button hold detected - triggering state change");

    hold_processed = true;

    // Notify LED of state change
    status_led_state_changed();

    // Get current state and send appropriate event
    wifi_sm_state_t current_state = wifi_state_machine_get_state();

    if (current_state == WIFI_SM_AP_MODE) {
        // Exit AP mode
        wifi_state_machine_send_event(WIFI_SM_EVENT_BUTTON_RELEASE, NULL);
    } else {
        // Enter AP mode
        wifi_state_machine_send_event(WIFI_SM_EVENT_BUTTON_HOLD, NULL);
    }
}