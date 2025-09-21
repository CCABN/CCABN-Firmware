#include "captive_portal.h"
#include "wifi_manager.h"
#include "wifi_storage.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "esp_netif.h"
#include <string.h>
#include <cJSON.h>
#include <stdio.h>
#include <fcntl.h>

static const char *TAG = "CaptivePortal";
static httpd_handle_t server = NULL;
static wifi_ap_record_t* scanned_networks = NULL;
static uint16_t network_count = 0;
static TaskHandle_t dns_task_handle = NULL;
static bool dns_server_running = false;

// DNS server task for captive portal redirection
static void dns_server_task(void *pvParameters);

// HTTP handlers
static esp_err_t root_handler(httpd_req_t *req);
static esp_err_t scan_handler(httpd_req_t *req);
static esp_err_t connect_handler(httpd_req_t *req);
static esp_err_t catch_all_handler(httpd_req_t *req);

// HTML content loaded from SPIFFS
static char* html_template = nullptr;

// Load HTML template from SPIFFS
static bool load_html_template(void) {
    FILE* file = fopen("/spiffs/setup.html", "r");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open setup.html file from SPIFFS");
        return false;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Allocate memory for the content
    html_template = (char*)malloc(file_size + 1);
    if (!html_template) {
        ESP_LOGE(TAG, "Failed to allocate memory for HTML template");
        fclose(file);
        return false;
    }

    // Read file content
    size_t bytes_read = fread(html_template, 1, file_size, file);
    html_template[bytes_read] = '\0';

    fclose(file);
    ESP_LOGI(TAG, "Loaded HTML template from SPIFFS (%d bytes)", (int)bytes_read);
    return true;
}

void captive_portal_start(void) {
    ESP_LOGI(TAG, "Starting captive portal");

    // Load HTML template from SPIFFS, fallback to simple template if file not found
    if (!load_html_template()) {
        ESP_LOGW(TAG, "Using fallback HTML template");
        // Simple fallback HTML
        const char* fallback_html =
            "<!DOCTYPE html><html><body>"
            "<h1>CCABN Tracker Setup</h1>"
            "<p>HTML template file not found. Please upload setup.html to SPIFFS using 'pio run --target uploadfs'</p>"
            "</body></html>";

        size_t len = strlen(fallback_html);
        html_template = (char*)malloc(len + 1);
        if (html_template) {
            strcpy(html_template, fallback_html);
        }
    }

    // Start HTTP server
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    if (httpd_start(&server, &config) == ESP_OK) {
        // Main page handler
        httpd_uri_t root = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &root);

        // Network scan handler
        httpd_uri_t scan = {
            .uri = "/scan",
            .method = HTTP_GET,
            .handler = scan_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &scan);

        // WiFi connect handler
        httpd_uri_t connect = {
            .uri = "/connect",
            .method = HTTP_POST,
            .handler = connect_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &connect);

        // Catch-all handler for captive portal
        httpd_uri_t catch_all = {
            .uri = "/*",
            .method = HTTP_GET,
            .handler = catch_all_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &catch_all);

        ESP_LOGI(TAG, "HTTP server started");
    }

    // Start DNS server for captive portal redirection
    dns_server_running = true;
    xTaskCreate(dns_server_task, "dns_server", 4096, NULL, 5, &dns_task_handle);
}

void captive_portal_stop(void) {
    ESP_LOGI(TAG, "Stopping captive portal");

    // Stop DNS server task
    if (dns_task_handle != NULL) {
        dns_server_running = false;
        vTaskDelay(100 / portTICK_PERIOD_MS); // Give task time to see the flag

        // Force delete the task if it's still running
        if (eTaskGetState(dns_task_handle) != eDeleted) {
            vTaskDelete(dns_task_handle);
        }
        dns_task_handle = NULL;
    }

    if (server) {
        httpd_stop(server);
        server = NULL;
    }

    if (scanned_networks) {
        free(scanned_networks);
        scanned_networks = NULL;
        network_count = 0;
    }

    if (html_template) {
        free(html_template);
        html_template = NULL;
    }
}

void captive_portal_scan_networks(void) {
    ESP_LOGI(TAG, "Scanning for WiFi networks");

    // Get current WiFi mode
    wifi_mode_t current_mode;
    esp_wifi_get_mode(&current_mode);

    // WiFi scanning requires STA mode, so temporarily switch to APSTA mode
    if (current_mode == WIFI_MODE_AP) {
        ESP_LOGI(TAG, "Switching to APSTA mode for scanning");
        esp_wifi_set_mode(WIFI_MODE_APSTA);
        vTaskDelay(100 / portTICK_PERIOD_MS); // Give time for mode change
    }

    wifi_scan_config_t scan_config = {};
    esp_err_t scan_result = esp_wifi_scan_start(&scan_config, true);

    if (scan_result != ESP_OK) {
        ESP_LOGE(TAG, "WiFi scan failed: %s", esp_err_to_name(scan_result));
        // Restore original mode if scan failed
        if (current_mode == WIFI_MODE_AP) {
            esp_wifi_set_mode(WIFI_MODE_AP);
        }
        network_count = 0;
        return;
    }

    esp_err_t count_result = esp_wifi_scan_get_ap_num(&network_count);
    if (count_result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get AP count: %s", esp_err_to_name(count_result));
        network_count = 0;
    }

    if (scanned_networks) {
        free(scanned_networks);
        scanned_networks = NULL;
    }

    if (network_count > 0) {
        scanned_networks = (wifi_ap_record_t*)malloc(sizeof(wifi_ap_record_t) * network_count);
        if (scanned_networks) {
            esp_err_t records_result = esp_wifi_scan_get_ap_records(&network_count, scanned_networks);
            if (records_result != ESP_OK) {
                ESP_LOGE(TAG, "Failed to get AP records: %s", esp_err_to_name(records_result));
                free(scanned_networks);
                scanned_networks = NULL;
                network_count = 0;
            }
        } else {
            ESP_LOGE(TAG, "Failed to allocate memory for scanned networks");
            network_count = 0;
        }
    }

    // Restore original WiFi mode
    if (current_mode == WIFI_MODE_AP) {
        ESP_LOGI(TAG, "Restoring AP mode");
        esp_wifi_set_mode(WIFI_MODE_AP);
    }

    ESP_LOGI(TAG, "Found %d networks", network_count);
}

// HTTP Handlers
static esp_err_t root_handler(httpd_req_t *req) {
    if (html_template) {
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, html_template, strlen(html_template));
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to load HTML template");
    }
    return ESP_OK;
}

static esp_err_t scan_handler(httpd_req_t *req) {
    captive_portal_scan_networks();

    cJSON *json = cJSON_CreateArray();

    for (int i = 0; i < network_count; i++) {
        cJSON *network = cJSON_CreateObject();
        cJSON_AddStringToObject(network, "ssid", (char*)scanned_networks[i].ssid);
        cJSON_AddNumberToObject(network, "rssi", scanned_networks[i].rssi);
        cJSON_AddItemToArray(json, network);
    }

    char *json_string = cJSON_Print(json);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_string, strlen(json_string));

    free(json_string);
    cJSON_Delete(json);

    return ESP_OK;
}

static esp_err_t connect_handler(httpd_req_t *req) {
    char content[256];
    int content_length = req->content_len;

    if (content_length >= sizeof(content)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content too large");
        return ESP_FAIL;
    }

    int received = httpd_req_recv(req, content, content_length);
    if (received <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive data");
        return ESP_FAIL;
    }

    content[received] = '\0';

    cJSON *json = cJSON_Parse(content);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *ssid_json = cJSON_GetObjectItem(json, "ssid");
    cJSON *password_json = cJSON_GetObjectItem(json, "password");

    if (!cJSON_IsString(ssid_json) || !cJSON_IsString(password_json)) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing ssid or password");
        return ESP_FAIL;
    }

    const char* ssid = ssid_json->valuestring;
    const char* password = password_json->valuestring;

    ESP_LOGI(TAG, "Connecting to WiFi: %s", ssid);

    // Save credentials to NVS
    if (wifi_storage_save_credentials(ssid, password)) {
        ESP_LOGI(TAG, "WiFi credentials saved successfully");
    } else {
        ESP_LOGW(TAG, "Failed to save WiFi credentials");
    }

    wifi_manager_connect_sta(ssid, password);

    httpd_resp_send(req, "OK", 2);
    cJSON_Delete(json);

    return ESP_OK;
}

static esp_err_t catch_all_handler(httpd_req_t *req) {
    // Redirect all requests to root for captive portal
    return root_handler(req);
}

// DNS Server for captive portal redirection
static void dns_server_task(void *pvParameters) {
    struct sockaddr_in server_addr;
    int sock = -1;
    char rx_buffer[128];

    ESP_LOGI(TAG, "DNS server task started");

    while (dns_server_running) {
        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }

        // Set socket to non-blocking to allow checking the running flag
        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);

        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(53);

        int err = bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (err < 0) {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
            close(sock);
            sock = -1;
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        ESP_LOGI(TAG, "DNS Server listening on port 53");

        while (dns_server_running) {
            struct sockaddr_storage source_addr;
            socklen_t socklen = sizeof(source_addr);
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

            if (len < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // No data available, check if we should continue running
                    vTaskDelay(10 / portTICK_PERIOD_MS);
                    continue;
                } else {
                    ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                    break;
                }
            }

            if (!dns_server_running) {
                break;
            }

            // Create DNS response redirecting to 192.168.4.1
            char dns_reply[128];
            int reply_len = 12;
            memcpy(dns_reply, rx_buffer, 12);
            dns_reply[2] = 0x81;
            dns_reply[3] = 0x80;
            dns_reply[7] = 0x01;

            memcpy(dns_reply + 12, rx_buffer + 12, len - 12);
            reply_len += len - 12;

            dns_reply[reply_len++] = 0xC0;
            dns_reply[reply_len++] = 0x0C;
            dns_reply[reply_len++] = 0x00;
            dns_reply[reply_len++] = 0x01;
            dns_reply[reply_len++] = 0x00;
            dns_reply[reply_len++] = 0x01;
            dns_reply[reply_len++] = 0x00;
            dns_reply[reply_len++] = 0x00;
            dns_reply[reply_len++] = 0x00;
            dns_reply[reply_len++] = 0x3C;
            dns_reply[reply_len++] = 0x00;
            dns_reply[reply_len++] = 0x04;
            dns_reply[reply_len++] = 192;
            dns_reply[reply_len++] = 168;
            dns_reply[reply_len++] = 4;
            dns_reply[reply_len++] = 1;

            sendto(sock, dns_reply, reply_len, 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
        }

        if (sock >= 0) {
            close(sock);
            sock = -1;
        }
        break; // Exit the outer loop when dns_server_running becomes false
    }

    ESP_LOGI(TAG, "DNS server task stopped");
    vTaskDelete(NULL);
}