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

    // Start the AP with the capture portal. this will be the hardest part.
    bool WiFiManager::startAP() {

        /// The capture portal should just use a singular html file. you can look at the one in /data for reference but it needs heavy modification.
        /// scanning for networks should run entirely in the background so that requests can be processed at the same time it's scanning
        /// have a notifier to know whether or not network scanning is in effect
        /// scans should run automatically every 5 seconds. if a scan command is sent and its not already scanning, a new scan should start.
        /// the user should know that the credentials have been saved correctly (eg. a 10 second countdown on a seconds page that says "credentials saved. exiting in X seconds.)
        /// more specifics are probably necessary but i cant think about them now. ask me later.

        /// make sure to set the credentials by the end of the function. use saveCredentials and set the actual credentials variable.
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
