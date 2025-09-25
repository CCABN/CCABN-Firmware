#include "wifi_manager.h"
#include <HWCDC.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <SPIFFS.h>

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
        server = nullptr;
        dnsServer = nullptr;
        preferences = new Preferences();
        credentials.exists = false;
        credentials.isPassword = false;
        credentials.ssid = "";
        credentials.password = "";

        // Initialize SPIFFS for web content
        if (!SPIFFS.begin(true)) {
            addError("Failed to mount SPIFFS");
        }

        // Load stored credentials
        getCredentials();
    }

    WiFiManager::~WiFiManager() {
        stopAP();
        if (preferences) {
            delete preferences;
            preferences = nullptr;
        }
    }

    WiFiCredentials WiFiManager::getCredentials() {
        preferences->begin("wifi", true); // true = read-only

        String storedSsid = preferences->getString("ssid", "");
        String storedPassword = preferences->getString("password", "");

        preferences->end();

        credentials.ssid = storedSsid;
        credentials.password = storedPassword;
        credentials.exists = (storedSsid.length() > 0);
        credentials.isPassword = (storedPassword.length() > 0);

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

    void WiFiManager::saveCredentials(WiFiCredentials newCredentials) {
        preferences->begin("wifi", false); // false = read-write

        preferences->putString("ssid", newCredentials.ssid);
        preferences->putString("password", newCredentials.password);

        preferences->end();

        // Update local credentials
        credentials = newCredentials;
        credentials.exists = (newCredentials.ssid.length() > 0);
        credentials.isPassword = (newCredentials.password.length() > 0);

        Serial.println("WiFi credentials saved");
    }

    // Start the AP with the capture portal. this will be the hardest part.
    bool WiFiManager::startAP() {
        // Stop any existing connections
        WiFi.disconnect();
        stopAP();

        // Set WiFi mode to support both AP and Station operations
        Serial.println("[WiFiManager] Setting WiFi mode to WIFI_AP_STA for concurrent operations");
        WiFi.mode(WIFI_AP_STA);
        delay(100); // Give time for mode change

        // Start Access Point
        String apNameStr = String(apName.c_str());
        bool apSuccess = WiFi.softAP(apNameStr);

        Serial.println("[WiFiManager] WiFi mode after AP start: " + String(WiFi.getMode()));

        if (!apSuccess) {
            addError("Failed to start Access Point");
            return false;
        }

        apMode = true;
        status = WiFiStatus::WIFI_AP_MODE;

        Serial.println("Access Point started: " + apNameStr);
        Serial.println("AP IP address: " + WiFi.softAPIP().toString());

        // Initialize DNS Server for captive portal
        dnsServer = new DNSServer();
        dnsServer->start(53, "*", WiFi.softAPIP());

        // Initialize Web Server
        server = new WebServer(80);

        server->on("/", [this]() { handleRoot(); });
        server->on("/connect", HTTP_POST, [this]() { handleConnect(); });
        server->on("/networks", [this]() { handleNetworks(); });

        // Handle common captive portal detection requests
        server->on("/generate_204", [this]() { handleRoot(); }); // Android captive portal detection
        server->on("/fwlink", [this]() { handleRoot(); }); // Windows captive portal detection
        server->on("/hotspot-detect.html", [this]() { handleRoot(); }); // iOS captive portal detection
        server->on("/library/test/success.html", [this]() { handleRoot(); }); // iOS alternative
        server->on("/connecttest.txt", [this]() { handleRoot(); }); // Windows 10
        server->on("/redirect", [this]() { handleRoot(); }); // Windows 8/10

        // Handle favicon
        server->on("/favicon.ico", [this]() {
            // Return empty 404 for favicon to stop repeated requests
            server->send(404, "text/plain", "");
        });

        server->onNotFound([this]() { handleNotFound(); });

        server->begin();
        Serial.println("Captive portal web server started");

        // Delay first scan to let AP mode stabilize
        Serial.println("[WiFiManager] Delaying first scan by 2 seconds for AP stabilization");
        lastScanTime = millis() + 2000; // Start first scan in 2 seconds

        return true;
    }

    bool WiFiManager::connectToWiFi(const WiFiCredentials &creds) {
        if (creds.ssid.length() == 0) {
            addError("No SSID provided");
            status = WiFiStatus::WIFI_FAILED;
            return false;
        }

        status = WiFiStatus::WIFI_CONNECTING;
        Serial.println("Connecting to WiFi: " + creds.ssid);

        // Disconnect first if already connected
        if (WiFi.status() == WL_CONNECTED) {
            WiFi.disconnect();
            delay(1000);
        }

        // Begin WiFi connection
        if (creds.isPassword && creds.password.length() > 0) {
            WiFi.begin(creds.ssid.c_str(), creds.password.c_str());
        } else {
            WiFi.begin(creds.ssid.c_str());
        }

        // Wait for connection with timeout
        unsigned long startTime = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < connectionTimeout) {
            delay(500);
            Serial.print(".");
        }

        if (WiFi.status() == WL_CONNECTED) {
            status = WiFiStatus::WIFI_CONNECTED;
            Serial.println();
            Serial.println("WiFi connected!");
            Serial.println("IP address: " + WiFi.localIP().toString());
            return true;
        } else {
            status = WiFiStatus::WIFI_FAILED;
            addError("Failed to connect to WiFi: " + creds.ssid);
            Serial.println();
            Serial.println("WiFi connection failed");
            return false;
        }
    }

    bool WiFiManager::connectToWiFi() {
        if (credentials.exists) {
            return connectToWiFi(credentials);
        } else {
            Serial.println("No Credentials");
            return false;
        }
    }

    void WiFiManager::addError(const String& error) {
        errors.push_back(error.c_str());
        if (autoPrintErrors) {
            Serial.println(error);
        }
    }

    bool WiFiManager::isConnected() {
        return (WiFi.status() == WL_CONNECTED && status == WiFiStatus::WIFI_CONNECTED);
    }

    void WiFiManager::stopAP() {
        if (server) {
            server->stop();
            delete server;
            server = nullptr;
        }
        if (dnsServer) {
            dnsServer->stop();
            delete dnsServer;
            dnsServer = nullptr;
        }
        if (apMode) {
            WiFi.softAPdisconnect(true);
            apMode = false;
            status = WiFiStatus::WIFI_DISCONNECTED;
            Serial.println("Access Point stopped");
        }
    }

    void WiFiManager::handleRoot() {
        String html = getHTMLTemplate();

        // Serve immediate response with scanning message or cached results
        String networksHtml = "";
        if (scanInProgress) {
            networksHtml = "<div class='scanning'><div class='spinner'></div>Scanning for networks...</div>";
        } else if (scanComplete) {
            networksHtml = cachedNetworksHtml;
        } else {
            networksHtml = "<div class='no-networks'>Starting network scan...</div>";
        }

        html.replace("{{NETWORKS_LIST}}", networksHtml);
        server->send(200, "text/html", html);
    }

    void WiFiManager::handleConnect() {
        String ssid = server->arg("ssid");
        String password = server->arg("password");

        if (ssid.length() == 0) {
            server->send(400, "text/html", "<h1>Error</h1><p>SSID is required</p><a href='/'>Back</a>");
            return;
        }

        // Save credentials
        WiFiCredentials newCreds;
        newCreds.ssid = ssid;
        newCreds.password = password;
        newCreds.exists = true;
        newCreds.isPassword = (password.length() > 0);

        saveCredentials(newCreds);

        // Send success page
        String html = "<!DOCTYPE html><html><head><title>WiFi Setup</title></head><body>";
        html += "<h1>Credentials Saved</h1>";
        html += "<p>WiFi credentials have been saved successfully.</p>";
        html += "<p>SSID: " + ssid + "</p>";
        html += "<p>The device will now attempt to connect to the network.</p>";
        html += "<p>You can close this page.</p>";
        html += "</body></html>";

        server->send(200, "text/html", html);

        // Stop AP mode and attempt connection
        delay(2000); // Give time for the response to be sent
        stopAP();
    }

    void WiFiManager::handleNotFound() {
        // Log unhandled requests for debugging
        String method = (server->method() == HTTP_GET) ? "GET" :
                       (server->method() == HTTP_POST) ? "POST" :
                       (server->method() == HTTP_PUT) ? "PUT" :
                       (server->method() == HTTP_DELETE) ? "DELETE" : "UNKNOWN";

        Serial.println("[WiFiManager] Unhandled request: " + method + " " + server->uri());
        Serial.println("[WiFiManager] Headers: " + String(server->headers()));
        for (int i = 0; i < server->headers(); i++) {
            Serial.println("[WiFiManager]   " + server->headerName(i) + ": " + server->header(i));
        }
        Serial.println("[WiFiManager] Args: " + String(server->args()));
        for (int i = 0; i < server->args(); i++) {
            Serial.println("[WiFiManager]   " + server->argName(i) + " = " + server->arg(i));
        }
        Serial.println("[WiFiManager] Client IP: " + server->client().remoteIP().toString());
        Serial.println("---");

        // Redirect to root for captive portal
        server->sendHeader("Location", "http://" + WiFi.softAPIP().toString(), true);
        server->send(302, "text/plain", "");
    }

    String WiFiManager::scanNetworks() {
        String networksHtml = "";

        Serial.println("Scanning networks...");
        int numNetworks = WiFi.scanNetworks();

        if (numNetworks == 0) {
            networksHtml = "<div class='no-networks'>No networks found</div>";
        } else {
            for (int i = 0; i < numNetworks; i++) {
                String ssid = WiFi.SSID(i);
                int32_t rssi = WiFi.RSSI(i);
                bool isSecured = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);

                String signalStrength = "";
                if (rssi > -50) signalStrength = "Excellent";
                else if (rssi > -60) signalStrength = "Good";
                else if (rssi > -70) signalStrength = "Fair";
                else signalStrength = "Weak";

                networksHtml += "<div class='network' data-ssid='" + ssid + "' data-secured='" + (isSecured ? "true" : "false") + "'>";
                networksHtml += "<span>" + ssid + "</span>";
                networksHtml += "<span class='signal-strength'>" + signalStrength;
                if (isSecured) {
                    networksHtml += " <span class='lock-icon'>🔒</span>";
                }
                networksHtml += "</span>";
                networksHtml += "</div>";
            }
        }

        return networksHtml;
    }

    String WiFiManager::getHTMLTemplate() {
        // Try to load from SPIFFS first
        if (SPIFFS.exists("/wifiportal.html")) {
            File file = SPIFFS.open("/wifiportal.html", "r");
            if (file) {
                String content = file.readString();
                file.close();
                return content;
            }
        }

        // Fallback to basic HTML if file not found
        String html = "<!DOCTYPE html><html><head><title>WiFi Setup</title>";
        html += "<style>";
        html += "body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }";
        html += ".container { max-width: 400px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }";
        html += ".network { padding: 10px; margin: 5px 0; border: 1px solid #ddd; border-radius: 4px; cursor: pointer; background: #f9f9f9; display: flex; justify-content: space-between; align-items: center; }";
        html += ".network:hover { background: #e9e9e9; }";
        html += ".network.selected { background: #007cba; color: white; }";
        html += ".signal-strength { font-size: 12px; color: #666; }";
        html += ".network.selected .signal-strength { color: #ccc; }";
        html += ".lock-icon { color: #ff6b6b; margin-left: 5px; }";
        html += ".network.selected .lock-icon { color: #ffcccb; }";
        html += ".scanning { padding: 20px; text-align: center; color: #007cba; display: flex; align-items: center; justify-content: center; gap: 10px; }";
        html += ".spinner { width: 20px; height: 20px; border: 2px solid #f3f3f3; border-top: 2px solid #007cba; border-radius: 50%; animation: spin 1s linear infinite; }";
        html += "@keyframes spin { 0% { transform: rotate(0deg); } 100% { transform: rotate(360deg); } }";
        html += ".form-group { margin: 15px 0; }";
        html += "label { display: block; margin-bottom: 5px; font-weight: bold; }";
        html += "input[type='text'], input[type='password'] { width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 4px; font-size: 16px; box-sizing: border-box; }";
        html += "button { width: 100%; padding: 12px; background: #007cba; color: white; border: none; border-radius: 4px; font-size: 16px; cursor: pointer; }";
        html += "button:hover { background: #005a8b; }";
        html += ".refresh { background: #28a745; margin-bottom: 20px; }";
        html += ".refresh:hover { background: #1e7e34; }";
        html += ".networks-list { max-height: 200px; overflow-y: auto; border: 1px solid #ddd; border-radius: 4px; margin-bottom: 15px; }";
        html += ".no-networks { padding: 20px; text-align: center; color: #666; font-style: italic; }";
        html += "</style>";
        html += "</head><body>";
        html += "<div class='container'>";
        html += "<h1>WiFi Configuration</h1>";
        html += "<button onclick='refreshNetworks()' class='refresh'>🔄 Refresh Networks</button>";
        html += "<form action='/connect' method='POST'>";
        html += "<div class='form-group'>";
        html += "<label>Available Networks:</label>";
        html += "<div class='networks-list' id='networks-list'>{{NETWORKS_LIST}}</div>";
        html += "</div>";
        html += "<div class='form-group'>";
        html += "<label for='ssid'>Network Name (SSID):</label>";
        html += "<input type='text' id='ssid' name='ssid' required>";
        html += "</div>";
        html += "<div class='form-group'>";
        html += "<label for='password'>Password:</label>";
        html += "<input type='password' id='password' name='password'>";
        html += "<small style='color: #666;'>Leave blank for open networks</small>";
        html += "</div>";
        html += "<button type='submit'>Connect</button>";
        html += "</form>";
        html += "</div>";
        html += "<script>";
        html += "function refreshNetworks() {";
        html += "  fetch('/networks').then(response => response.text()).then(data => {";
        html += "    document.getElementById('networks-list').innerHTML = data;";
        html += "    addNetworkClickHandlers();";
        html += "  });";
        html += "}";
        html += "function addNetworkClickHandlers() {";
        html += "  document.querySelectorAll('.network').forEach(network => {";
        html += "    network.onclick = function() {";
        html += "      document.querySelectorAll('.network').forEach(n => n.classList.remove('selected'));";
        html += "      this.classList.add('selected');";
        html += "      const ssid = this.dataset.ssid;";
        html += "      if (ssid) {";
        html += "        document.getElementById('ssid').value = ssid;";
        html += "        if (this.dataset.secured === 'true') {";
        html += "          document.getElementById('password').focus();";
        html += "        } else {";
        html += "          document.getElementById('password').value = '';";
        html += "        }";
        html += "      }";
        html += "    };";
        html += "  });";
        html += "}";
        html += "addNetworkClickHandlers();";
        html += "setInterval(refreshNetworks, 5000);"; // Auto-refresh every 5 seconds
        html += "</script>";
        html += "</body></html>";

        return html;
    }

    void WiFiManager::loop() {
        if (apMode && server && dnsServer) {
            dnsServer->processNextRequest();
            server->handleClient();

            // Handle background WiFi scanning
            checkScanStatus();
        }
    }

    void WiFiManager::clearCredentials() {
        preferences->begin("wifi", false); // false = read-write
        preferences->clear(); // Clear all preferences in this namespace
        preferences->end();

        // Update local credentials
        credentials.ssid = "";
        credentials.password = "";
        credentials.exists = false;
        credentials.isPassword = false;

        Serial.println("[WiFiManager] WiFi credentials cleared");
    }

    void WiFiManager::startBackgroundScan() {
        if (!scanInProgress && apMode) {
            Serial.println("[WiFiManager] Starting background WiFi scan");

            // Use synchronous scanning since async fails in AP mode
            // Keep it fast with shorter timeout to minimize web server blocking
            int syncResult = WiFi.scanNetworks(false, false, false, 200); // 200ms per channel max

            if (syncResult >= 0) {
                Serial.println("[WiFiManager] Background scan completed, found " + String(syncResult) + " networks");
                scanInProgress = false;
                scanComplete = true;
                lastNetworkCount = syncResult;

                // Build the cached networks HTML
                cachedNetworksHtml = buildNetworksHtml();
                lastScanTime = millis();

                // Reset scan interval to normal on success
                scanInterval = 15000; // Scan every 15 seconds when working
            } else if (syncResult == WIFI_SCAN_FAILED) {
                Serial.println("[WiFiManager] Background scan failed - will retry less frequently");
                scanInProgress = false;
                scanComplete = false;
                lastScanTime = millis();

                // Increase retry interval on failure, max 60 seconds
                scanInterval = min(scanInterval * 2, 60000UL);
                Serial.println("[WiFiManager] Next scan retry in " + String(scanInterval/1000) + " seconds");
            } else {
                Serial.println("[WiFiManager] Unexpected scan result: " + String(syncResult));
                scanInProgress = false;
                scanComplete = false;
                lastScanTime = millis();
            }
        }
    }

    void WiFiManager::checkScanStatus() {
        // Since we're using synchronous scanning, just check if it's time for a new scan
        if (!scanInProgress && millis() > lastScanTime && (millis() - lastScanTime) > scanInterval) {
            startBackgroundScan();
        }
    }

    String WiFiManager::buildNetworksHtml() {
        String networksHtml = "";
        int numNetworks = WiFi.scanComplete();

        if (numNetworks == 0) {
            networksHtml = "<div class='no-networks'>No networks found</div>";
        } else if (numNetworks > 0) {
            for (int i = 0; i < numNetworks; i++) {
                String ssid = WiFi.SSID(i);
                int32_t rssi = WiFi.RSSI(i);
                bool isSecured = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);

                String signalStrength = "";
                if (rssi > -50) signalStrength = "Excellent";
                else if (rssi > -60) signalStrength = "Good";
                else if (rssi > -70) signalStrength = "Fair";
                else signalStrength = "Weak";

                networksHtml += "<div class='network' data-ssid='" + ssid + "' data-secured='" + (isSecured ? "true" : "false") + "'>";
                networksHtml += "<span>" + ssid + "</span>";
                networksHtml += "<span class='signal-strength'>" + signalStrength;
                if (isSecured) {
                    networksHtml += " <span class='lock-icon'>🔒</span>";
                }
                networksHtml += "</span>";
                networksHtml += "</div>";
            }
        }

        return networksHtml;
    }

    void WiFiManager::handleNetworks() {
        // AJAX endpoint to get current network scan results
        String response = "";

        if (scanInProgress) {
            response = "<div class='scanning'><div class='spinner'></div>Scanning for networks...</div>";
        } else if (scanComplete) {
            response = cachedNetworksHtml;
        } else {
            response = "<div class='no-networks'>Starting network scan...</div>";
        }

        server->send(200, "text/html", response);
    }
}
