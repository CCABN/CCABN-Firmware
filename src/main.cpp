#include <Arduino.h>
#include "wifi_manager.h"

using namespace CCABN;

WiFiManager wifiManager;

void setup() {
    Serial.begin(115200);
    delay(10000);

    Serial.println("==========Serial Monitor Started==========");

    wifiManager.apName = "My Device Setup";
    wifiManager.autoPrintErrors = true;
    WiFiCredentials credentials = wifiManager.getCredentials();

    while (wifiManager.status != WiFiStatus::WIFI_CONNECTED) {
        if (credentials.exists) {
            Serial.println("Connecting to WiFi");
            bool connected = wifiManager.connectToWiFi(credentials);
            if (!connected) {
                Serial.println("Failed to connect. Please check credentials.");
                break; // Exit loop to prevent infinite retry
            }
        } else {
            Serial.println("No WiFi Credentials Found");
            Serial.println("Starting Access Point");
            wifiManager.startAP();

            // Wait for credentials to be configured via captive portal
            Serial.println("Waiting for WiFi configuration via captive portal...");
            while (wifiManager.status == WiFiStatus::WIFI_AP_MODE) {
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