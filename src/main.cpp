#include <Arduino.h>
#include "../lib/wifi_manager.h"

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
            wifiManager.connectToWiFi(credentials);
        } else {
            Serial.println("No WiFi Credentials Found");
            Serial.println("Starting Access Point");
            wifiManager.startAP();
        }
    }


}

void loop() {
    // Your main application code here
    delay(1000);
}