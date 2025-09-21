#ifndef WIFI_STORAGE_H
#define WIFI_STORAGE_H

#include <stdbool.h>

// WiFi credentials structure
typedef struct {
    char ssid[32];
    char password[64];
} wifi_credentials_t;

// Function declarations
bool wifi_storage_load_credentials(wifi_credentials_t* credentials);
bool wifi_storage_save_credentials(const char* ssid, const char* password);
bool wifi_storage_clear_credentials(void);
bool wifi_storage_has_credentials(void);

#endif // WIFI_STORAGE_H