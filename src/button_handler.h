#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <stdbool.h>
#include <stdint.h>

// Button configuration
typedef struct {
    int gpio_pin;
    bool active_high;          // true if button is pressed when GPIO is high
    uint32_t hold_time_ms;     // Time to hold for state change (default: 3000)
    uint32_t debounce_ms;      // Debounce time (default: 50)
} button_config_t;

// Function declarations
bool button_handler_init(const button_config_t* config);
void button_handler_deinit(void);

// Status
bool button_handler_is_initialized(void);
bool button_handler_is_pressed(void);

#endif // BUTTON_HANDLER_H