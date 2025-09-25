#pragma once
#include <Arduino.h>
#include <string>
#include <vector>
#include <set>

// Forward declarations
class WebServer;
class DNSServer;
class Preferences;

namespace CCABN {

    enum WiFiStatus {
        WIFI_CONNECTED,
        WIFI_CONNECTING,
        WIFI_DISCONNECTED,
        WIFI_FAILED,
        WIFI_AP_MODE
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
        WebServer* server;
        DNSServer* dnsServer;
        Preferences* preferences;
        unsigned long connectionTimeout = 15000;
        bool apMode = false;

        // Network caching system
        std::set<String> cachedNetworks;
        bool scanInProgress = false;
        bool scanComplete = false;
        bool firstScanPending = false;
        unsigned long firstScanScheduledTime = 0;
        unsigned long lastScanTime = 0;
        unsigned long scanInterval = 15000; // Scan every 15 seconds
        String cachedNetworksHtml = "";
        int lastNetworkCount = 0;


    public:
        std::string apName = "Default AP Name";
        bool autoPrintErrors = false;
        WiFiStatus status = WiFiStatus::WIFI_DISCONNECTED;

        WiFiManager();
        ~WiFiManager();
        WiFiCredentials getCredentials();
        void printErrors();
        std::vector<std::string> getErrors();
        void printCredentials();
        void saveCredentials(WiFiCredentials credentials);
        bool startAP();
        bool connectToWiFi(const WiFiCredentials &credentials);
        bool connectToWiFi();
        void stopAP();
        bool isConnected();
        void loop();
        void clearCredentials();

    private:
        void handleRoot();
        void handleConnect();
        void handleNotFound();
        void handleNetworks();
        void handleScanStatus();
        void handleExit();
        void handleStaticFile();
        String scanNetworks();
        String getHTMLTemplate();
        void addError(const String& error);
        void startBackgroundScan();
        void checkScanStatus();
        String buildNetworksHtml();
        void clearNetworkCache();
    };
}
