#ifndef CAPTIVE_PORTAL_V2_H
#define CAPTIVE_PORTAL_V2_H

#include "esp_http_server.h"

// Captive portal configuration
typedef struct {
    const char* device_name;
    uint16_t http_port;
    uint16_t dns_port;
    const char* ap_ip;
} captive_portal_config_t;

// Callback function types
typedef void (*credentials_saved_callback_t)(const char* ssid, const char* password);

// Function declarations
bool captive_portal_init(const captive_portal_config_t* config);
void captive_portal_deinit();

bool captive_portal_start();
void captive_portal_stop();

bool captive_portal_is_running();

// Set callback for when credentials are saved
void captive_portal_set_credentials_callback(credentials_saved_callback_t callback);

#endif // CAPTIVE_PORTAL_V2_H