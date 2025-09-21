#ifndef STATUS_LED_H
#define STATUS_LED_H

#include <stdbool.h>
#include <stdint.h>

// LED patterns corresponding to WiFi states
typedef enum {
    LED_PATTERN_SOLID,         // Solid on - WiFi connected
    LED_PATTERN_PULSE,         // Breathing/pulsing - connecting/disconnected
    LED_PATTERN_BLINK,         // Blinking - AP mode
    LED_PATTERN_OFF,           // Forced off - button pressed
} led_pattern_t;

// LED configuration
typedef struct {
    int gpio_pin;              // GPIO pin number
    uint32_t pwm_frequency;    // PWM frequency in Hz (default: 1000)
    uint8_t ledc_channel;      // LEDC channel to use (0-7)
    uint8_t ledc_timer;        // LEDC timer to use (0-3)
} status_led_config_t;

// Function declarations
bool status_led_init(const status_led_config_t* config);
void status_led_deinit(void);

// Pattern control - main API
void status_led_set_pattern(led_pattern_t pattern);
led_pattern_t status_led_get_current_pattern(void);

// Button interaction
void status_led_button_pressed(void);    // Turn off LED, save current pattern
void status_led_button_released(void);   // Restore saved pattern
void status_led_state_changed(void);     // Called when state change completes

// Status
bool status_led_is_initialized(void);

#endif // STATUS_LED_H