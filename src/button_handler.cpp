#include "button_handler.h"
#include "wifi_state_machine.h"
#include "status_led.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/task.h"

static const char *TAG = "ButtonHandler";

// Configuration and state
static button_config_t button_config = {};
static bool is_initialized = false;
static bool button_pressed = false;
static bool hold_processed = false;

// Timers and tasks
static TimerHandle_t hold_timer = nullptr;
static TaskHandle_t event_task_handle = nullptr;

// Forward declarations
static void IRAM_ATTR button_isr_handler(void* arg);
static void button_hold_timer_callback(TimerHandle_t timer);
static void event_handler_task(void* pvParameters);

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
    ret = gpio_isr_handler_add(static_cast<gpio_num_t>(button_config.gpio_pin), button_isr_handler, nullptr);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add ISR handler: %s", esp_err_to_name(ret));
        return false;
    }

    // Create hold timer
    hold_timer = xTimerCreate(
        "button_hold",
        pdMS_TO_TICKS(button_config.hold_time_ms),
        pdFALSE,  // One-shot timer
        nullptr,
        button_hold_timer_callback
    );

    if (!hold_timer) {
        ESP_LOGE(TAG, "Failed to create hold timer");
        gpio_isr_handler_remove(static_cast<gpio_num_t>(button_config.gpio_pin));
        return false;
    }

    // Create event handler task
    if (xTaskCreate(event_handler_task, "btn_events", 4096, nullptr, 5, &event_task_handle) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create event handler task");
        xTimerDelete(hold_timer, 0);
        gpio_isr_handler_remove(static_cast<gpio_num_t>(button_config.gpio_pin));
        return false;
    }

    // Initialize state
    button_pressed = false;
    hold_processed = false;

    is_initialized = true;
    ESP_LOGI(TAG, "Button handler initialized successfully");

    return true;
}

void button_handler_deinit() {
    if (!is_initialized) {
        return;
    }

    ESP_LOGI(TAG, "Deinitializing button handler");

    // Remove ISR handler
    gpio_isr_handler_remove(static_cast<gpio_num_t>(button_config.gpio_pin));

    // Delete timer
    if (hold_timer) {
        xTimerStop(hold_timer, 0);
        xTimerDelete(hold_timer, 0);
        hold_timer = nullptr;
    }

    // Delete event task
    if (event_task_handle) {
        vTaskDelete(event_task_handle);
        event_task_handle = nullptr;
    }

    is_initialized = false;
    ESP_LOGI(TAG, "Button handler deinitialized");
}

bool button_handler_is_initialized() {
    return is_initialized;
}

bool button_handler_is_pressed() {
    if (!is_initialized) {
        return false;
    }

    const int level = gpio_get_level(static_cast<gpio_num_t>(button_config.gpio_pin));
    return button_config.active_high ? (level == 1) : (level == 0);
}

// Private functions
static void button_isr_handler(void* arg) {
    BaseType_t higher_priority_task_woken = pdFALSE;
    const bool current_pressed = button_handler_is_pressed();

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
        // if (!hold_processed) {
        //     // Button was released before hold time - no state change
        // } else {
        //     // Button was released after hold time - send release event
        //     // Note: We can't call the state machine from ISR, so we'll need
        //     // a task for this. For now, the state machine will handle this
        //     // through the LED system integration.
        // }
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

    // Get current state and determine event
    const wifi_sm_state_t current_state = wifi_state_machine_get_state();
    uint32_t event_notification;

    if (current_state == WIFI_SM_AP_MODE) {
        // Exit AP mode
        event_notification = WIFI_SM_EVENT_BUTTON_RELEASE;
    } else {
        // Enter AP mode
        event_notification = WIFI_SM_EVENT_BUTTON_HOLD;
    }

    // Send notification to event handler task (safe from timer context)
    BaseType_t higher_priority_task_woken = pdFALSE;
    if (event_task_handle != nullptr) {
        xTaskNotifyFromISR(event_task_handle, event_notification, eSetValueWithOverwrite, &higher_priority_task_woken);
        portYIELD_FROM_ISR(higher_priority_task_woken);
    }
}

static void event_handler_task(void* pvParameters) {
    uint32_t event_notification;

    ESP_LOGI(TAG, "Event handler task started");

    while (true) {
        // Wait for notification from timer callback
        if (xTaskNotifyWait(0, UINT32_MAX, &event_notification, portMAX_DELAY) == pdTRUE) {
            // Send event to state machine in proper task context
            ESP_LOGI(TAG, "Processing state machine event: %lu", event_notification);
            wifi_state_machine_send_event(static_cast<wifi_sm_event_t>(event_notification), nullptr);
        }
    }
}