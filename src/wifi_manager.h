#pragma once
#include <Arduino.h>
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

    /**
     * @brief Callback function type for WiFi connection state changes
     * @param connected True if WiFi connected, false if disconnected
     */
    typedef void (*WiFiConnectionCallback)(bool connected);

    /**
     * @brief Container for WiFi network credentials
     */
    class WiFiCredentials {
    public:
        String ssid;         ///< Network SSID name
        String password;     ///< Network password (empty if open network)
        bool exists;         ///< True if credentials exist in storage
        bool hasPassword;    ///< True if network requires password

        /**
         * @brief Print credentials to log for debugging
         */
        void print() const;
    };

    /**
     * @brief WiFi connection manager with captive portal configuration
     *
     * This class handles WiFi connections and provides a captive portal interface
     * for configuring WiFi credentials when no valid connection exists. The captive
     * portal is self-contained and blocking, returning only when credentials are
     * received or timeout occurs.
     *
     * Usage flow:
     * 1. Check for existing credentials with getCredentials()
     * 2. If credentials exist, call connectToWiFi()
     * 3. If no credentials, call startAP() which will block until configured
     * 4. Set connection callback to be notified of connection state changes
     */
    class WiFiManager {
    private:
        WiFiCredentials credentials;
        WebServer* server;
        DNSServer* dnsServer;
        Preferences* preferences;
        unsigned long connectionTimeout = 15000;
        // No AP timeout - runs indefinitely until credentials received
        bool apMode = false;
        bool credentialsReceived = false;
        WiFiConnectionCallback connectionCallback = nullptr;

        // Network caching system
        std::set<String> cachedNetworks;
        bool scanInProgress = false;
        bool scanComplete = false;
        bool firstScanPending = false;
        unsigned long firstScanScheduledTime = 0;
        unsigned long lastScanTime = 0;
        unsigned long scanInterval = 15000; // Scan every 15 seconds
        String cachedNetworksHtml = "";


    public:
        String apName = "Default AP Name";
        WiFiStatus status = WIFI_DISCONNECTED;

        /**
         * @brief Constructor - initializes WiFi manager and loads stored credentials
         */
        WiFiManager();

        /**
         * @brief Destructor - cleans up resources and stops AP if active
         */
        ~WiFiManager();

        /**
         * @brief Retrieve stored WiFi credentials from flash memory
         * @return WiFiCredentials object with loaded credentials and status
         */
        WiFiCredentials getCredentials();

        /**
         * @brief Print current credentials to log for debugging
         */
        void printCredentials() const;

        /**
         * @brief Save WiFi credentials to flash memory
         * @param credentials The WiFi credentials to store
         */
        void saveCredentials(const WiFiCredentials& credentials);

        /**
         * @brief Start captive portal AP mode and block until credentials received
         *
         * This method is blocking and will:
         * 1. Start an access point with the configured name
         * 2. Start a web server with captive portal
         * 3. Handle client requests for network scanning and credential entry
         * 4. Block until credentials are received or timeout (5 minutes)
         * 5. Clean up and stop AP mode before returning
         *
         * @return true if credentials were received, false on timeout
         */
        bool startAP();

        /**
         * @brief Connect to WiFi using provided credentials
         * @param credentials The WiFi network credentials to use
         * @return true if connection successful, false if failed
         */
        bool connectToWiFi(const WiFiCredentials &credentials);

        /**
         * @brief Connect to WiFi using stored credentials
         * @return true if connection successful, false if failed or no credentials
         */
        bool connectToWiFi();

        /**
         * @brief Stop access point mode and clean up resources
         */
        void stopAP();

        /**
         * @brief Check if currently connected to WiFi
         * @return true if connected, false otherwise
         */
        bool isConnected() const;

        /**
         * @brief Process captive portal requests (internal use only)
         *
         * This method handles web server and DNS server requests during AP mode.
         * It's called internally by startAP() and shouldn't be called manually.
         */
        void loop();

        /**
         * @brief Clear stored WiFi credentials from flash memory
         */
        void clearCredentials();

        /**
         * @brief Set callback function to be notified of connection state changes
         * @param callback Function to call when WiFi connection state changes
         */
        void setConnectionCallback(WiFiConnectionCallback callback);

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
        static String getFallbackHTML();
        void startBackgroundScan();
        void checkScanStatus();
        String buildNetworksHtml();
        void clearNetworkCache();
    };
}
