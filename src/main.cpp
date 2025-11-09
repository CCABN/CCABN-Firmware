#include <Arduino.h>
#include "wifi_manager.h"
#include <WiFi.h>

using namespace CCABN;

WiFiManager wifiManager;

void setup() {
    Serial.begin(115200);
    delay(2000);

    // Print startup banner
    for (int i = 0; i < 5; i++) {
        Serial.println("\n\n=== ESP32-S3 STARTING ===");
        delay(200);
    }

    log_i("[Main] CCABN Firmware started");
    Serial.println("CCABN Firmware started");
    Serial.printf("Chip: %s Rev %d\n", ESP.getChipModel(), ESP.getChipRevision());
    Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("PSRAM Size: %d bytes\n", ESP.getPsramSize());

    // Setup WiFi
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    const String macSuffix = mac.substring(0, 6);
    wifiManager.apName = "CCABN_TRACKER_" + macSuffix;

    // wifiManager.clearCredentials();
    WiFiCredentials credentials = wifiManager.getCredentials();

    while (wifiManager.status != WIFI_CONNECTED) {
        if (credentials.exists) {
            log_i("[Main] Attempting to connect to stored WiFi network");
            const bool connected = wifiManager.connectToWiFi(credentials);
            if (!connected) {
                log_e("[Main] Failed to connect with stored credentials");
            }
        } else {
            log_i("[Main] No WiFi credentials found");
            log_i("[Main] Starting Access Point for configuration");

            wifiManager.startAP();

            log_i("[Main] Credentials received, attempting to connect");
            credentials = wifiManager.getCredentials();
        }
    }
}

void loop() {
    Serial.println("Loop running");
    delay(5000);
}