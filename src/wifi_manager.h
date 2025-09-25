#pragma once
#include <Arduino.h>
#include <string>
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
        bool hasPassword;

        void print() const;
    };

    class WiFiManager {
    private:
        WiFiCredentials credentials;
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
        WiFiStatus status = WIFI_DISCONNECTED;

        WiFiManager();
        ~WiFiManager();
        WiFiCredentials getCredentials();
        void printCredentials() const;
        void saveCredentials(const WiFiCredentials& credentials);
        bool startAP();
        bool connectToWiFi(const WiFiCredentials &credentials);
        bool connectToWiFi();
        void stopAP();
        bool isConnected() const;
        void loop();
        void clearCredentials();

    private:
        void handleRoot() const;
        void handleConnect();
        void handleNotFound() const;
        void handleNetworks();
        void handleScanStatus() const;
        void handleExit();
        void handleStaticFile() const;
        static String scanNetworks();
        static String getHTMLTemplate();
        void startBackgroundScan();
        void checkScanStatus();
        String buildNetworksHtml();
        void clearNetworkCache();
    };
}
