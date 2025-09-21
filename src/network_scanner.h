#ifndef NETWORK_SCANNER_H
#define NETWORK_SCANNER_H

#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>
#include <stdbool.h>

#define MAX_SCAN_RESULTS 50

// Network scan result structure
typedef struct {
    char ssid[33];
    int8_t rssi;
    wifi_auth_mode_t authmode;
} network_scan_result_t;

// Scanner configuration
typedef struct {
    bool continuous_scan;      // Whether to scan continuously
    uint32_t scan_interval_ms; // Interval between scans in continuous mode
    bool active;               // Whether scanner is currently active
} network_scanner_config_t;

// Function declarations
void network_scanner_init(void);
void network_scanner_deinit(void);

// Scanning control
bool network_scanner_start_continuous(uint32_t interval_ms);
void network_scanner_stop_continuous(void);
bool network_scanner_scan_once(void);

// Results access
uint16_t network_scanner_get_result_count(void);
const network_scan_result_t* network_scanner_get_results(void);
bool network_scanner_get_results_json(char* buffer, size_t buffer_size);

// Status
bool network_scanner_is_active(void);
bool network_scanner_is_scanning(void);

#endif // NETWORK_SCANNER_H