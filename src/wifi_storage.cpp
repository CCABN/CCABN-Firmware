#include "wifi_storage.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "WiFiStorage";
static const char *STORAGE_NAMESPACE = "wifi_config";
static const char *SSID_KEY = "ssid";
static const char *PASSWORD_KEY = "password";

bool wifi_storage_load_credentials(wifi_credentials_t* credentials) {
    if (!credentials) {
        return false;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &nvs_handle);

    if (err != ESP_OK) {
        ESP_LOGD(TAG, "No WiFi credentials found");
        return false;
    }

    size_t ssid_len = sizeof(credentials->ssid);
    size_t password_len = sizeof(credentials->password);

    err = nvs_get_str(nvs_handle, SSID_KEY, credentials->ssid, &ssid_len);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        return false;
    }

    err = nvs_get_str(nvs_handle, PASSWORD_KEY, credentials->password, &password_len);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        return false;
    }

    nvs_close(nvs_handle);

    ESP_LOGI(TAG, "Loaded WiFi credentials for SSID: %s", credentials->ssid);
    return true;
}

bool wifi_storage_save_credentials(const char* ssid, const char* password) {
    if (!ssid || !password) {
        return false;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_set_str(nvs_handle, SSID_KEY, ssid);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error saving SSID: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }

    err = nvs_set_str(nvs_handle, PASSWORD_KEY, password);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error saving password: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error committing to NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }

    nvs_close(nvs_handle);

    ESP_LOGI(TAG, "WiFi credentials saved for SSID: %s", ssid);
    return true;
}

bool wifi_storage_clear_credentials(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);

    if (err != ESP_OK) {
        ESP_LOGD(TAG, "No credentials to clear");
        return true;
    }

    nvs_erase_key(nvs_handle, SSID_KEY);
    nvs_erase_key(nvs_handle, PASSWORD_KEY);
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    ESP_LOGI(TAG, "WiFi credentials cleared");
    return true;
}

bool wifi_storage_has_credentials(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &nvs_handle);

    if (err != ESP_OK) {
        return false;
    }

    size_t required_size = 0;
    err = nvs_get_str(nvs_handle, SSID_KEY, NULL, &required_size);
    nvs_close(nvs_handle);

    return (err == ESP_OK && required_size > 0);
}