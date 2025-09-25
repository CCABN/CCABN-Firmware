#include "wifi_manager.h"
#include <HWCDC.h>
#include <WiFi.h>

// TODO: This whole file could use a lot of polishing. please do more than the bare minimum.

namespace CCABN {
    void WiFiCredentials::print() {
        Serial.println("Printing WiFi Credentials:");
        if (exists) {
            Serial.println("SSID: " + ssid);
            if (isPassword) {
                Serial.println("Password: " + password);
            } else {
                Serial.println("No Password");
            }

        } else {
            Serial.println("No Credentials");
        }
    }

    WiFiManager::WiFiManager() {
        credentials.exists = false;
        credentials.isPassword = false;
        credentials.ssid = "";
        credentials.password = "";
        getCredentials();
    }

    WiFiCredentials WiFiManager::getCredentials() {
        // try to find credentials saved on device (use spiffs probably.). then set the credentials variable to contain these values
        return credentials;
    }

    void WiFiManager::printErrors() {
        Serial.println("Printing Errors:");
        for (int i = 0; i < errors.size(); i++) {
            Serial.println(errors[i].c_str());
        }
    }

    std::vector<std::string> WiFiManager::getErrors() {
        return errors;
    }

    void WiFiManager::printCredentials() {
        credentials.print();
    }

    void WiFiManager::saveCredentials(WiFiCredentials credentials) {
        // Save the wifi credentials to spiffs
    }

    bool WiFiManager::startAP() {
        // Start the AP with the capture portal. this will be the hardest part.
        // Set the credentials
    }

    bool WiFiManager::connectToWiFi(WiFiCredentials credentials) {
        WiFi.begin(credentials.ssid, credentials.password); // make it more robust than this
    }

    bool WiFiManager::connectToWiFi() {
        if (credentials.exists) {
            return connectToWiFi(credentials);
        } else {
            Serial.println("No Credentials");
            return false;
        }
    }
}
