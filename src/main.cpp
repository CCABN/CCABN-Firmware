#include <Arduino.h>
#include "wifi_manager.h"
#include <WiFi.h>
#include <esp_log.h>

using namespace CCABN;

WiFiManager wifiManager;

void setup() {
    Serial.begin(115200);
    // Gives time for serial monitor to connect
    // delay(10000);

    log_i("[Main] CCABN Firmware started");

    String mac = WiFi.macAddress();
    mac.replace(":", "");
    const String macSuffix = mac.substring(0, 6);
    wifiManager.apName = "CCABN_TRACKER_" + macSuffix;

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

            // startAP() now blocks until credentials are received (no timeout)
            wifiManager.startAP();

            log_i("[Main] Credentials received, attempting to connect");
            // Reload credentials after AP mode
            credentials = wifiManager.getCredentials();
        }
    }


}

void loop() {
    // Your main application code here
    delay(1000);
}