#include <Arduino.h>
#include "wifi_manager.h"
#include <WiFi.h>

using namespace CCABN;

WiFiManager wifiManager;

void setup() {
    Serial.begin(115200);
    // Gives time for serial monitor to connect
    // delay(10000);

    Serial.println("==========Serial Monitor Started==========");

    String mac = WiFi.macAddress();
    mac.replace(":", "");
    const String macSuffix = mac.substring(0, 6);
    wifiManager.apName = ("CCABN_TRACKER_" + macSuffix).c_str();

    // Clear credentials to force reconfiguration
    wifiManager.clearCredentials();

    WiFiCredentials credentials = wifiManager.getCredentials();

    while (wifiManager.status != WIFI_CONNECTED) {
        if (credentials.exists) {
            Serial.println("Connecting to WiFi");
            const bool connected = wifiManager.connectToWiFi(credentials);
            if (!connected) {
                Serial.println("Failed to connect. Please check credentials.");
            }
        } else {
            Serial.println("No WiFi Credentials Found");
            Serial.println("Starting Access Point");
            wifiManager.startAP();

            // Wait for credentials to be configured via captive portal
            Serial.println("Waiting for WiFi configuration via captive portal...");
            while (wifiManager.status == WIFI_AP_MODE) {
                wifiManager.loop();
                delay(100);
            }

            // Reload credentials after AP mode
            credentials = wifiManager.getCredentials();
        }
    }


}

void loop() {
    // Your main application code here
    delay(1000);
}