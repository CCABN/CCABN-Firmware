#include "status_led.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

static const char *TAG = "StatusLED";

// Configuration
static status_led_config_t led_config = {0};
static bool is_initialized = false;

// State management
static led_pattern_t current_pattern = LED_PATTERN_OFF;
static led_pattern_t saved_pattern = LED_PATTERN_OFF;
static bool button_override_active = false;
static bool ignore_button_hold = false;

// Animation state
static TimerHandle_t led_timer = NULL;
static uint8_t pulse_brightness = 50;    // Start at minimum for pulse
static int8_t pulse_direction = 1;       // 1 = increasing, -1 = decreasing
static bool blink_state = false;         // true = on, false = off

// Forward declarations
static void led_timer_callback(TimerHandle_t timer);
static void set_led_brightness(uint8_t brightness);
static void update_led_output(void);

bool status_led_init(const status_led_config_t* config) {
    if (is_initialized) {
        ESP_LOGW(TAG, "LED already initialized");
        return false;
    }

    if (!config) {
        ESP_LOGE(TAG, "Invalid configuration");
        return false;
    }

    ESP_LOGI(TAG, "Initializing status LED on GPIO %d", config->gpio_pin);

    // Copy configuration
    led_config = *config;

    // Set defaults if not specified
    if (led_config.pwm_frequency == 0) {
        led_config.pwm_frequency = 1000;
    }

    // Configure LEDC timer
    ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = (ledc_timer_t)led_config.ledc_timer,
        .freq_hz = led_config.pwm_frequency,
        .clk_cfg = LEDC_AUTO_CLK
    };

    esp_err_t err = ledc_timer_config(&timer_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC timer: %s", esp_err_to_name(err));
        return false;
    }

    // Configure LEDC channel
    ledc_channel_config_t channel_config = {
        .gpio_num = led_config.gpio_pin,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = (ledc_channel_t)led_config.ledc_channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = (ledc_timer_t)led_config.ledc_timer,
        .duty = 0,  // Start with LED off
        .hpoint = 0
    };

    err = ledc_channel_config(&channel_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC channel: %s", esp_err_to_name(err));
        return false;
    }

    // Create timer for LED patterns
    led_timer = xTimerCreate(
        "led_timer",
        pdMS_TO_TICKS(50),  // Default 50ms period
        pdTRUE,             // Auto-reload
        NULL,
        led_timer_callback
    );

    if (!led_timer) {
        ESP_LOGE(TAG, "Failed to create LED timer");
        return false;
    }

    // Initialize state
    current_pattern = LED_PATTERN_OFF;
    saved_pattern = LED_PATTERN_OFF;
    button_override_active = false;
    ignore_button_hold = false;
    pulse_brightness = 50;
    pulse_direction = 1;
    blink_state = false;

    is_initialized = true;
    ESP_LOGI(TAG, "Status LED initialized successfully");

    return true;
}

void status_led_deinit(void) {
    if (!is_initialized) {
        return;
    }

    ESP_LOGI(TAG, "Deinitializing status LED");

    // Stop timer
    if (led_timer) {
        xTimerStop(led_timer, 0);
        xTimerDelete(led_timer, 0);
        led_timer = NULL;
    }

    // Turn off LED
    set_led_brightness(0);

    is_initialized = false;
    ESP_LOGI(TAG, "Status LED deinitialized");
}

void status_led_set_pattern(led_pattern_t pattern) {
    if (!is_initialized) {
        ESP_LOGW(TAG, "LED not initialized");
        return;
    }

    if (current_pattern == pattern) {
        return; // No change needed
    }

    ESP_LOGD(TAG, "Setting LED pattern: %d", pattern);

    current_pattern = pattern;

    // Stop current timer
    xTimerStop(led_timer, 0);

    // Configure for new pattern
    switch (pattern) {
        case LED_PATTERN_SOLID:
            set_led_brightness(255);  // Full brightness
            break;

        case LED_PATTERN_PULSE:
            pulse_brightness = 50;    // Start at minimum
            pulse_direction = 1;      // Start increasing
            xTimerChangePeriod(led_timer, pdMS_TO_TICKS(20), 0);  // 20ms for smooth pulse
            xTimerStart(led_timer, 0);
            break;

        case LED_PATTERN_BLINK:
            blink_state = false;      // Start with LED off
            xTimerChangePeriod(led_timer, pdMS_TO_TICKS(500), 0); // 500ms blink period
            xTimerStart(led_timer, 0);
            break;

        case LED_PATTERN_OFF:
            set_led_brightness(0);    // Turn off
            break;

        default:
            ESP_LOGW(TAG, "Unknown LED pattern: %d", pattern);
            break;
    }
}

led_pattern_t status_led_get_current_pattern(void) {
    return current_pattern;
}

void status_led_button_pressed(void) {
    if (!is_initialized) {
        return;
    }

    if (!button_override_active) {
        ESP_LOGD(TAG, "Button pressed - LED off");
        saved_pattern = current_pattern;
        button_override_active = true;

        // Stop timer and turn off LED immediately
        xTimerStop(led_timer, 0);
        set_led_brightness(0);
    }
}

void status_led_button_released(void) {
    if (!is_initialized) {
        return;
    }

    if (button_override_active) {
        ESP_LOGD(TAG, "Button released - restoring pattern");
        button_override_active = false;
        ignore_button_hold = false;  // Reset hold ignore flag

        // Restore the saved pattern
        led_pattern_t pattern_to_restore = saved_pattern;
        current_pattern = LED_PATTERN_OFF;  // Force pattern change
        status_led_set_pattern(pattern_to_restore);
    }
}

void status_led_state_changed(void) {
    if (!is_initialized) {
        return;
    }

    if (button_override_active) {
        ESP_LOGD(TAG, "State changed - LED on, ignoring further holds");
        // Turn LED on briefly to signal state change
        set_led_brightness(255);
        ignore_button_hold = true;  // Ignore button holds until release
    }
}

bool status_led_is_initialized(void) {
    return is_initialized;
}

// Private functions
static void led_timer_callback(TimerHandle_t timer) {
    if (!is_initialized || button_override_active) {
        return;
    }

    switch (current_pattern) {
        case LED_PATTERN_PULSE:
            // Smooth pulsing between 50 and 255
            pulse_brightness += (pulse_direction * 5);

            if (pulse_brightness >= 255) {
                pulse_brightness = 255;
                pulse_direction = -1;
            } else if (pulse_brightness <= 50) {
                pulse_brightness = 50;
                pulse_direction = 1;
            }

            set_led_brightness(pulse_brightness);
            break;

        case LED_PATTERN_BLINK:
            // Toggle between on and off every 500ms
            blink_state = !blink_state;
            set_led_brightness(blink_state ? 255 : 0);
            break;

        default:
            // Solid and off patterns don't need timer updates
            break;
    }
}

static void set_led_brightness(uint8_t brightness) {
    if (!is_initialized) {
        return;
    }

    ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)led_config.ledc_channel, brightness);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)led_config.ledc_channel);
}

static void update_led_output(void) {
    // This function is reserved for future use if we need to add
    // additional processing before setting LED brightness
    // Currently, set_led_brightness handles direct output
}