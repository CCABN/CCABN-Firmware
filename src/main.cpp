#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "lwip/dns.h"

static const char *TAG = "CCABN_TRACKER";

#define WIFI_SSID      "CCABN_Tracker_1"
#define WIFI_PASS      ""
#define WIFI_CHANNEL   1
#define MAX_STA_CONN   4

static httpd_handle_t server = NULL;

const char* html_page =
"<!DOCTYPE html>"
"<html>"
"<head>"
"    <title>CCABN Tracker</title>"
"    <meta name='viewport' content='width=device-width, initial-scale=1'>"
"    <style>"
"        body { font-family: Arial, sans-serif; text-align: center; padding: 50px; }"
"        h1 { color: #333; }"
"    </style>"
"</head>"
"<body>"
"    <h1>Hello World!</h1>"
"    <p>Welcome to CCABN Tracker (platformIO edition)</p>"
"    <p>Device is running successfully.</p>"
"</body>"
"</html>";

static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_page, strlen(html_page));
    return ESP_OK;
}

static esp_err_t catch_all_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_page, strlen(html_page));
    return ESP_OK;
}

static void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = root_get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &root);

        httpd_uri_t catch_all = {
            .uri       = "/*",
            .method    = HTTP_GET,
            .handler   = catch_all_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &catch_all);

        ESP_LOGI(TAG, "Web server started successfully");
    } else {
        ESP_LOGI(TAG, "Error starting server!");
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "Station " MACSTR " joined, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "Station " MACSTR " left, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {};
    strcpy((char*)wifi_config.ap.ssid, WIFI_SSID);
    wifi_config.ap.ssid_len = strlen(WIFI_SSID);
    strcpy((char*)wifi_config.ap.password, WIFI_PASS);
    wifi_config.ap.channel = WIFI_CHANNEL;
    wifi_config.ap.max_connection = MAX_STA_CONN;
    wifi_config.ap.authmode = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi AP started. SSID:%s channel:%d", WIFI_SSID, WIFI_CHANNEL);
}

static void dns_server_task(void *pvParameters)
{
    struct sockaddr_in server_addr;
    int sock;
    char rx_buffer[128];
    char addr_str[128];
    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_UDP;

    while (1) {
        sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }

        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(53);

        int err = bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (err < 0) {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        }

        ESP_LOGI(TAG, "DNS Server listening on port 53");

        while (1) {
            struct sockaddr_storage source_addr;
            socklen_t socklen = sizeof(source_addr);
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

            if (len < 0) {
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                break;
            } else {
                inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);

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
        }

        if (sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}

extern "C" void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "Starting CCABN Tracker ESP32-S3");

    wifi_init_softap();

    start_webserver();

    xTaskCreate(dns_server_task, "dns_server", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "CCABN Tracker initialized successfully");
    ESP_LOGI(TAG, "Connect to WiFi: %s", WIFI_SSID);
    ESP_LOGI(TAG, "Open browser to: http://192.168.4.1");
}