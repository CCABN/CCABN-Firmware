#pragma once
#include <string>
#include <vector>
#include <WString.h>

namespace CCABN {

    enum WiFiStatus {
        WIFI_CONNECTED,
        WIFI_CONNECTING,
        WIFI_DISCONNECTED,
        WIFI_FAILED
    };

    class WiFiCredentials {
    public:
        String ssid;
        String password;
        bool exists;
        bool isPassword;

        void print();
    };

    class WiFiManager {
    private:
        WiFiCredentials credentials;
        std::vector<std::string> errors;

    public:
        std::string apName = "Default AP Name";
        bool autoPrintErrors = false;
        WiFiStatus status = WiFiStatus::WIFI_DISCONNECTED;

        WiFiManager();
        WiFiCredentials getCredentials();
        void printErrors();
        std::vector<std::string> getErrors();
        void printCredentials();

        /**
         * claude i want you to add documentation to all the functions and variables like this please
         * @param credentials credentials to use
         */
        void saveCredentials(WiFiCredentials credentials);
        bool startAP();
        bool connectToWiFi(WiFiCredentials credentials);
        bool connectToWiFi();
    };
}
