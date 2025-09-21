#include "wifi_state_machine.h"
#include "button_handler.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "CCABN";

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Starting CCABN Tracker ESP32-S3");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize SPIFFS filesystem
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = nullptr,
        .max_files = 5,
        .format_if_mount_failed = false
    };
    esp_err_t spiffs_ret = esp_vfs_spiffs_register(&conf);
    if (spiffs_ret != ESP_OK) {
        ESP_LOGW(TAG, "SPIFFS not available, using fallback HTML");
    } else {
        ESP_LOGI(TAG, "SPIFFS mounted successfully");
    }

    // Initialize Wi-Fi state machine
    wifi_state_machine_init();

    // Initialize button handler
    button_config_t button_config = {
        .gpio_pin = 2,           // BUTTON_PIN from original code
        .active_high = true,     // Button is active high
        .hold_time_ms = 3000,    // 3 seconds for state change
        .debounce_ms = 50        // 50ms debounce
    };

    if (!button_handler_init(&button_config)) {
        ESP_LOGE(TAG, "Failed to initialize button handler");
        return;
    }

    ESP_LOGI(TAG, "CCABN Tracker initialized successfully");
    ESP_LOGI(TAG, "Hold button for 3 seconds to toggle WiFi setup mode");

    // Main loop - just keep the task alive
    // All functionality is now handled by the state machine and modules
    // ReSharper disable once CppDFAEndlessLoop
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10000)); // Sleep for 10 seconds
    }
}