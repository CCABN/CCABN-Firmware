#include "network_scanner.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "NetworkScanner";

// Scanner state
static network_scanner_config_t scanner_config = {};
static TaskHandle_t scan_task_handle = nullptr;
static bool task_should_run = false;

// Scan results
static network_scan_result_t scan_results[MAX_SCAN_RESULTS];
static uint16_t result_count = 0;
static bool scan_in_progress = false;

// Forward declarations
static void continuous_scan_task(void* parameter);
static bool perform_scan(void);

void network_scanner_init(void) {
    ESP_LOGI(TAG, "Initializing network scanner");

    scanner_config.continuous_scan = false;
    scanner_config.scan_interval_ms = 4000;
    scanner_config.active = false;

    result_count = 0;
    scan_in_progress = false;
    task_should_run = false;
    scan_task_handle = nullptr;

    ESP_LOGI(TAG, "Network scanner initialized");
}

void network_scanner_deinit(void) {
    ESP_LOGI(TAG, "Deinitializing network scanner");

    network_scanner_stop_continuous();

    scanner_config.active = false;
    result_count = 0;
    scan_in_progress = false;

    ESP_LOGI(TAG, "Network scanner deinitialized");
}

bool network_scanner_start_continuous(uint32_t interval_ms) {
    if (scanner_config.active) {
        ESP_LOGW(TAG, "Scanner already active");
        return false;
    }

    ESP_LOGI(TAG, "Starting continuous scanning (interval: %lu ms)", interval_ms);

    scanner_config.continuous_scan = true;
    scanner_config.scan_interval_ms = interval_ms;
    scanner_config.active = true;
    task_should_run = true;

    BaseType_t result = xTaskCreate(
        continuous_scan_task,
        "net_scan",
        4096,
        nullptr,
        4,
        &scan_task_handle
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create scan task");
        scanner_config.active = false;
        task_should_run = false;
        return false;
    }

    return true;
}

void network_scanner_stop_continuous(void) {
    if (!scanner_config.active) {
        return;
    }

    ESP_LOGI(TAG, "Stopping continuous scanning");

    task_should_run = false;
    scanner_config.continuous_scan = false;

    if (scan_task_handle != nullptr) {
        // Wait for task to finish gracefully
        vTaskDelay(pdMS_TO_TICKS(100));

        // Force delete if still running
        if (eTaskGetState(scan_task_handle) != eDeleted) {
            vTaskDelete(scan_task_handle);
        }
        scan_task_handle = nullptr;
    }

    scanner_config.active = false;
    ESP_LOGI(TAG, "Continuous scanning stopped");
}

bool network_scanner_scan_once(void) {
    if (scan_in_progress) {
        ESP_LOGW(TAG, "Scan already in progress");
        return false;
    }

    ESP_LOGI(TAG, "Performing one-time scan");
    return perform_scan();
}

uint16_t network_scanner_get_result_count(void) {
    return result_count;
}

const network_scan_result_t* network_scanner_get_results(void) {
    return scan_results;
}

bool network_scanner_get_results_json(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return false;
    }

    cJSON *json_array = cJSON_CreateArray();
    if (!json_array) {
        return false;
    }

    for (int i = 0; i < result_count; i++) {
        cJSON *network = cJSON_CreateObject();
        if (!network) {
            cJSON_Delete(json_array);
            return false;
        }

        cJSON_AddStringToObject(network, "ssid", scan_results[i].ssid);
        cJSON_AddNumberToObject(network, "rssi", scan_results[i].rssi);
        cJSON_AddItemToArray(json_array, network);
    }

    char *json_string = cJSON_Print(json_array);
    cJSON_Delete(json_array);

    if (!json_string) {
        return false;
    }

    if (strlen(json_string) >= buffer_size) {
        free(json_string);
        return false;
    }

    strcpy(buffer, json_string);
    free(json_string);

    return true;
}

bool network_scanner_is_active(void) {
    return scanner_config.active;
}

bool network_scanner_is_scanning(void) {
    return scan_in_progress;
}

// Private functions
static void continuous_scan_task(void* parameter) {
    ESP_LOGI(TAG, "Continuous scan task started");

    // ReSharper disable once CppDFALoopConditionNotUpdated
    while (task_should_run) {
        perform_scan();

        // Wait for next scan with early exit capability
        for (uint32_t i = 0; i < scanner_config.scan_interval_ms / 100 && task_should_run; i++) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    ESP_LOGI(TAG, "Continuous scan task stopped");
    vTaskDelete(nullptr);
}

static bool perform_scan(void) {
    if (scan_in_progress) {
        return false;
    }

    scan_in_progress = true;

    // Perform Wi-Fi scan
    wifi_scan_config_t scan_config = {};
    esp_err_t err = esp_wifi_scan_start(&scan_config, true);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WiFi scan failed: %s", esp_err_to_name(err));
        scan_in_progress = false;
        return false;
    }

    // Get scan results
    uint16_t ap_count = 0;
    err = esp_wifi_scan_get_ap_num(&ap_count);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get AP count: %s", esp_err_to_name(err));
        scan_in_progress = false;
        return false;
    }

    if (ap_count == 0) {
        result_count = 0;
        scan_in_progress = false;
        return true;
    }

    // Limit results to our buffer size
    if (ap_count > MAX_SCAN_RESULTS) {
        ap_count = MAX_SCAN_RESULTS;
    }

    wifi_ap_record_t *ap_records = static_cast<wifi_ap_record_t *>(malloc(sizeof(wifi_ap_record_t) * ap_count));
    if (!ap_records) {
        ESP_LOGE(TAG, "Failed to allocate memory for scan results");
        scan_in_progress = false;
        return false;
    }

    err = esp_wifi_scan_get_ap_records(&ap_count, ap_records);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get AP records: %s", esp_err_to_name(err));
        free(ap_records);
        scan_in_progress = false;
        return false;
    }

    // Copy results to our structure
    result_count = ap_count;
    for (int i = 0; i < ap_count; i++) {
        strncpy(scan_results[i].ssid, reinterpret_cast<char *>(ap_records[i].ssid), sizeof(scan_results[i].ssid) - 1);
        scan_results[i].ssid[sizeof(scan_results[i].ssid) - 1] = '\0';
        scan_results[i].rssi = ap_records[i].rssi;
        scan_results[i].authmode = ap_records[i].authmode;
    }

    free(ap_records);
    scan_in_progress = false;

    ESP_LOGD(TAG, "Scan completed, found %d networks", result_count);
    return true;
}