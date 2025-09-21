#include "captive_portal.h"
#include "network_scanner.h"
#include "wifi_storage.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "lwip/sockets.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"
#include <cstring>
#include <fcntl.h>

static const char *TAG = "CaptivePortalV2";

// Portal state
static captive_portal_config_t portal_config = {};
static bool is_initialized = false;
static bool is_running = false;

// Server handles
static httpd_handle_t http_server = nullptr;
static TaskHandle_t dns_task_handle = nullptr;
static bool dns_task_running = false;

// Callback
static credentials_saved_callback_t credentials_callback = nullptr;

// HTML template
static char* html_template = nullptr;

// Forward declarations
static void dns_server_task(void* parameter);
static esp_err_t root_handler(httpd_req_t *req);
static esp_err_t scan_handler(httpd_req_t *req);
static esp_err_t connect_handler(httpd_req_t *req);
static esp_err_t catch_all_handler(httpd_req_t *req);
static bool load_html_template();
static void cleanup_resources();

bool captive_portal_init(const captive_portal_config_t* config) {
    if (is_initialized) {
        ESP_LOGW(TAG, "Portal already initialized");
        return false;
    }

    if (!config) {
        ESP_LOGE(TAG, "Invalid configuration");
        return false;
    }

    ESP_LOGI(TAG, "Initializing captive portal");

    // Copy configuration
    portal_config = *config;
    is_initialized = true;
    is_running = false;

    // Initialize resources
    http_server = nullptr;
    dns_task_handle = nullptr;
    dns_task_running = false;
    credentials_callback = nullptr;
    html_template = nullptr;

    ESP_LOGI(TAG, "Captive portal initialized");
    return true;
}

void captive_portal_deinit() {
    if (!is_initialized) {
        return;
    }

    ESP_LOGI(TAG, "Deinitializing captive portal");

    captive_portal_stop();
    cleanup_resources();

    is_initialized = false;
    ESP_LOGI(TAG, "Captive portal deinitialized");
}

bool captive_portal_start() {
    if (!is_initialized) {
        ESP_LOGE(TAG, "Portal not initialized");
        return false;
    }

    if (is_running) {
        ESP_LOGW(TAG, "Portal already running");
        return true;
    }

    ESP_LOGI(TAG, "Starting captive portal");

    // Load HTML template
    if (!load_html_template()) {
        ESP_LOGW(TAG, "Using fallback HTML template");
        const char* fallback_html =
            "<!DOCTYPE html><html><body>"
            "<h1>CCABN Tracker Setup</h1>"
            "<p>WiFi configuration interface</p>"
            "</body></html>";

        size_t len = strlen(fallback_html);
        html_template = static_cast<char *>(malloc(len + 1));
        if (html_template) {
            strcpy(html_template, fallback_html);
        }
    }

    // Start HTTP server
    httpd_config_t http_config = HTTPD_DEFAULT_CONFIG();
    http_config.server_port = portal_config.http_port;
    http_config.lru_purge_enable = true;

    esp_err_t err = httpd_start(&http_server, &http_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(err));
        cleanup_resources();
        return false;
    }

    // Register handlers
    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler,
        .user_ctx = nullptr
    };
    httpd_register_uri_handler(http_server, &root_uri);

    httpd_uri_t scan_uri = {
        .uri = "/scan",
        .method = HTTP_GET,
        .handler = scan_handler,
        .user_ctx = nullptr
    };
    httpd_register_uri_handler(http_server, &scan_uri);

    httpd_uri_t connect_uri = {
        .uri = "/connect",
        .method = HTTP_POST,
        .handler = connect_handler,
        .user_ctx = nullptr
    };
    httpd_register_uri_handler(http_server, &connect_uri);

    // Captive portal detection handlers - return HTTP 204 to indicate no internet
    httpd_uri_t generate_204_uri = {
        .uri = "/generate_204",
        .method = HTTP_GET,
        .handler = [](httpd_req_t *req) -> esp_err_t {
            httpd_resp_set_status(req, "204 No Content");
            httpd_resp_send(req, nullptr, 0);
            return ESP_OK;
        },
        .user_ctx = nullptr
    };
    httpd_register_uri_handler(http_server, &generate_204_uri);

    httpd_uri_t gen_204_uri = {
        .uri = "/gen_204",
        .method = HTTP_GET,
        .handler = [](httpd_req_t *req) -> esp_err_t {
            httpd_resp_set_status(req, "204 No Content");
            httpd_resp_send(req, nullptr, 0);
            return ESP_OK;
        },
        .user_ctx = nullptr
    };
    httpd_register_uri_handler(http_server, &gen_204_uri);

    httpd_uri_t catch_all_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = catch_all_handler,
        .user_ctx = nullptr
    };
    httpd_register_uri_handler(http_server, &catch_all_uri);

    // Start DNS server
    dns_task_running = true;
    BaseType_t result = xTaskCreate(
        dns_server_task,
        "dns_server",
        4096,
        nullptr,
        5,
        &dns_task_handle
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create DNS task");
        httpd_stop(http_server);
        http_server = nullptr;
        cleanup_resources();
        return false;
    }

    is_running = true;
    ESP_LOGI(TAG, "Captive portal started on port %d", portal_config.http_port);
    return true;
}

void captive_portal_stop() {
    if (!is_running) {
        return;
    }

    ESP_LOGI(TAG, "Stopping captive portal");

    // Stop DNS server
    dns_task_running = false;
    if (dns_task_handle != nullptr) {
        vTaskDelay(pdMS_TO_TICKS(100)); // Give task time to see the flag

        if (eTaskGetState(dns_task_handle) != eDeleted) {
            vTaskDelete(dns_task_handle);
        }
        dns_task_handle = nullptr;
    }

    // Stop HTTP server
    if (http_server != nullptr) {
        httpd_stop(http_server);
        http_server = nullptr;
    }

    cleanup_resources();
    is_running = false;

    ESP_LOGI(TAG, "Captive portal stopped");
}

bool captive_portal_is_running() {
    return is_running;
}

void captive_portal_set_credentials_callback(credentials_saved_callback_t callback) {
    credentials_callback = callback;
}

// Private functions
static void cleanup_resources() {
    if (html_template) {
        free(html_template);
        html_template = nullptr;
    }
}

static bool load_html_template() {
    FILE* file = fopen("/spiffs/setup.html", "r");
    if (!file) {
        ESP_LOGD(TAG, "No HTML template file found in SPIFFS");
        return false;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    html_template = static_cast<char *>(malloc(file_size + 1));
    if (!html_template) {
        ESP_LOGE(TAG, "Failed to allocate memory for HTML template");
        fclose(file);
        return false;
    }

    size_t bytes_read = fread(html_template, 1, file_size, file);
    html_template[bytes_read] = '\0';

    fclose(file);
    ESP_LOGI(TAG, "Loaded HTML template (%d bytes)", (int)bytes_read);
    return true;
}

static esp_err_t root_handler(httpd_req_t *req) {
    if (html_template) {
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, html_template, static_cast<ssize_t>(strlen(html_template)));
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Template not available");
    }
    return ESP_OK;
}

static esp_err_t scan_handler(httpd_req_t *req) {
    char json_buffer[2048];

    if (network_scanner_get_results_json(json_buffer, sizeof(json_buffer))) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json_buffer, static_cast<ssize_t>(strlen(json_buffer)));
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to get scan results");
    }

    return ESP_OK;
}

static esp_err_t connect_handler(httpd_req_t *req) {
    char content[256];
    int content_length = static_cast<ssize_t>(req->content_len);

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

    ESP_LOGI(TAG, "Saving WiFi credentials: %s", ssid);

    // Save credentials to NVS
    // ReSharper disable once CppTooWideScope
    bool save_success = wifi_storage_save_credentials(ssid, password);

    if (save_success) {
        ESP_LOGI(TAG, "WiFi credentials saved successfully");
        httpd_resp_send(req, "Credentials saved. Exit AP mode to connect.",
                       static_cast<ssize_t>(strlen("Credentials saved. Exit AP mode to connect.")));

        // Call callback if set
        if (credentials_callback) {
            credentials_callback(ssid, password);
        }
    } else {
        ESP_LOGW(TAG, "Failed to save WiFi credentials");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save credentials");
    }

    cJSON_Delete(json);
    return ESP_OK;
}

// ReSharper disable once CppDFAConstantFunctionResult
static esp_err_t catch_all_handler(httpd_req_t *req) {
    // Redirect all requests to root for captive portal behavior
    return root_handler(req);
}

static void dns_server_task(void* parameter) {
    ESP_LOGI(TAG, "DNS server task started");

    struct sockaddr_in server_addr{};
    // ReSharper disable once CppDFAUnusedValue
    int sock = -1;
    char rx_buffer[128];

    while (dns_task_running) {
        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create DNS socket: errno %d", errno);
            break;
        }

        // Set socket to non-blocking
        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);

        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(portal_config.dns_port);

        int err = bind(sock, reinterpret_cast<struct sockaddr *>(&server_addr), sizeof(server_addr));
        if (err < 0) {
            ESP_LOGE(TAG, "DNS socket bind failed: errno %d", errno);
            close(sock);
            // ReSharper disable once CppDFAUnusedValue
            sock = -1;
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        ESP_LOGI(TAG, "DNS server listening on port %d", portal_config.dns_port);

        while (dns_task_running) {
            struct sockaddr_storage source_addr{};
            socklen_t socklen = sizeof(source_addr);
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0,
                              reinterpret_cast<struct sockaddr *>(&source_addr), &socklen);

            if (len < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    vTaskDelay(pdMS_TO_TICKS(10));
                    continue;
                } else {
                    ESP_LOGE(TAG, "DNS recvfrom failed: errno %d", errno);
                    break;
                }
            }

            if (!dns_task_running) {
                break;
            }

            // Create simple DNS response redirecting to AP IP
            char dns_reply[128];
            int reply_len = 12;
            memcpy(dns_reply, rx_buffer, 12);
            dns_reply[2] = 0x81;
            dns_reply[3] = 0x80;
            dns_reply[7] = 0x01;

            memcpy(dns_reply + 12, rx_buffer + 12, len - 12);
            reply_len += len - 12;

            // Add answer section pointing to 192.168.4.1
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

            sendto(sock, dns_reply, reply_len, 0, reinterpret_cast<struct sockaddr *>(&source_addr), socklen);
        }

        // ReSharper disable once CppDFAConstantConditions
        if (sock >= 0) {
            close(sock);
            // ReSharper disable once CppDFAUnusedValue
            sock = -1;
        }
        break;
    }

    ESP_LOGI(TAG, "DNS server task stopped");
    vTaskDelete(nullptr);
}