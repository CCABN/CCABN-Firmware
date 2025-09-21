#include "wifi_manager.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_spiffs.h"

static const char *TAG = "CCABN_TRACKER";

extern "C" void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize SPIFFS filesystem
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = false
    };
    esp_err_t spiffs_ret = esp_vfs_spiffs_register(&conf);
    if (spiffs_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SPIFFS (%s)", esp_err_to_name(spiffs_ret));
    } else {
        ESP_LOGI(TAG, "SPIFFS mounted successfully");
    }

    ESP_LOGI(TAG, "Starting CCABN Tracker ESP32-S3");
    ESP_LOGI(TAG, "Device: %s", wifi_manager_get_device_name());

    // Initialize WiFi manager (starts in AP mode)
    wifi_manager_init();

    ESP_LOGI(TAG, "CCABN Tracker initialized successfully");
    ESP_LOGI(TAG, "Hold button for 3 seconds to enter WiFi setup mode");

    // Temporary logging for testing serial monitor
    int counter = 0;
    while(1) {
        ESP_LOGI(TAG, "Device running normally - heartbeat %d", counter++);
        ESP_LOGI(TAG, "GPIO 2 state: %d", gpio_get_level((gpio_num_t)2));
        vTaskDelay(5000 / portTICK_PERIOD_MS); // 5 second delay
    }
}