#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

// ReSharper disable once CppUnusedIncludeDirective
#include <cstdint> // Despite what Clion says, this is needed

// Button configuration
typedef struct {
    int gpio_pin;
    bool active_high;          // true if button is pressed when GPIO is high
    uint32_t hold_time_ms;     // Time to hold for state change (default: 3000)
    uint32_t debounce_ms;      // Debounce time (default: 50)
} button_config_t;

// Function declarations
bool button_handler_init(const button_config_t* config);
void button_handler_deinit();

// Status
bool button_handler_is_initialized();
bool button_handler_is_pressed();

#endif // BUTTON_HANDLER_H