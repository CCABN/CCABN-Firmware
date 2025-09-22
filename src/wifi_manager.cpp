#include "wifi_manager.h"

CustomWiFiManager* CustomWiFiManager::instance = nullptr;

CustomWiFiManager::CustomWiFiManager()
    : _isConnected(false),
      _isConnecting(false),
      _credentialsExist(false),
      _lastReconnectAttempt(0),
      _connectionStartTime(0),
      _connectionCallback(nullptr) {
    instance = this;
}

void CustomWiFiManager::begin() {
    Serial.println("Starting WiFi Manager...");

    setupButton();
    setupWiFiManager();

    // Set up WiFi event handler
    WiFi.onEvent([](WiFiEvent_t event) {
        if (instance) {
            instance->onWiFiEvent(event);
        }
    });

    // Generate AP name from MAC address
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    String apName = "CCABN_TRACKER_" + mac.substring(6); // Last 3 bytes

    Serial.println("Attempting autoConnect with AP name: " + apName);

    // Use autoConnect - this handles everything automatically
    if (wifiManager.autoConnect(apName.c_str())) {
        Serial.println("WiFi connected successfully via autoConnect!");
        _isConnected = true;
        _credentialsExist = true;
        // Don't call callback here - WiFi events will handle it
    } else {
        Serial.println("AutoConnect failed - will retry in loop");
        _isConnected = false;
    }
}

void CustomWiFiManager::loop() {
    configButton.loop();

    if (_isConnecting) {
        // Check for connection timeout
        if (millis() - _connectionStartTime > CONNECTION_TIMEOUT) {
            Serial.println("Connection attempt timed out");
            _isConnecting = false;

            if (_credentialsExist) {
                // Schedule reconnection attempt
                _lastReconnectAttempt = millis();
            }
        }
    } else if (!_isConnected && _credentialsExist) {
        handleReconnection();
    }

    checkConnection();
}

bool CustomWiFiManager::isConnected() const {
    return _isConnected;
}

bool CustomWiFiManager::isConnecting() const {
    return _isConnecting;
}

void CustomWiFiManager::setConnectionCallback(ConnectionCallback callback) {
    _connectionCallback = callback;
}

void CustomWiFiManager::setupButton() {
    configButton.begin(BUTTON_PIN);
    configButton.setLongClickTime(BUTTON_HOLD_TIME);
    configButton.setLongClickHandler([this](Button2& btn) {
        this->handleButtonHold(btn);
    });
}

void CustomWiFiManager::setupWiFiManager() {
    // Configure WiFiManager
    wifiManager.setDebugOutput(true);
    wifiManager.setConfigPortalTimeout(300); // 5 minutes timeout
    wifiManager.setConnectTimeout(30); // 30 seconds to connect
    wifiManager.setAPCallback([](WiFiManager *myWiFiManager) {
        Serial.println("Entered config mode");
        Serial.println("AP IP address: " + WiFi.softAPIP().toString());
    });

    wifiManager.setSaveConfigCallback([]() {
        Serial.println("Configuration saved, restarting...");
        delay(1000);
        ESP.restart();
    });
}

void CustomWiFiManager::handleButtonHold(Button2& btn) {
    Serial.println("Button held for 3 seconds, starting config portal...");
    startConfigPortal();
}

void CustomWiFiManager::startConfigPortal() {
    Serial.println("Starting configuration portal...");

    // Disconnect from current WiFi if connected
    if (WiFi.isConnected()) {
        WiFi.disconnect();
    }

    _isConnected = false;
    _isConnecting = false;

    // Notify callback about disconnection
    if (_connectionCallback) {
        _connectionCallback(false);
    }

    // Generate AP name from MAC address
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    String apName = "CCABN_TRACKER_" + mac.substring(6); // Last 3 bytes

    Serial.println("Starting config portal with AP name: " + apName);

    // Start the config portal (blocking)
    bool result = wifiManager.startConfigPortal(apName.c_str());

    if (result) {
        Serial.println("WiFi configuration successful");
        _credentialsExist = true;
        // The save callback will restart the device
    } else {
        Serial.println("WiFi configuration failed or timed out");
        // If we had credentials before, try to reconnect
        if (_credentialsExist) {
            tryConnectToWiFi();
        } else {
            // No credentials and config failed, restart config portal
            Serial.println("No credentials available, restarting config portal...");
            delay(2000);
            startConfigPortal();
        }
    }
}

void CustomWiFiManager::tryConnectToWiFi() {
    if (_isConnecting) {
        return; // Already attempting connection
    }

    Serial.println("Attempting to connect to WiFi...");
    _isConnecting = true;
    _connectionStartTime = millis();

    WiFi.mode(WIFI_STA);
    WiFi.begin();
}

void CustomWiFiManager::handleReconnection() {
    unsigned long currentTime = millis();

    if (currentTime - _lastReconnectAttempt >= RECONNECT_INTERVAL) {
        Serial.println("Attempting to reconnect to WiFi...");
        _lastReconnectAttempt = currentTime;
        tryConnectToWiFi();
    }
}

void CustomWiFiManager::checkConnection() {
    bool currentlyConnected = (WiFi.status() == WL_CONNECTED);

    if (currentlyConnected != _isConnected) {
        _isConnected = currentlyConnected;
        _isConnecting = false;

        if (_isConnected) {
            Serial.println("WiFi connected!");
            Serial.println("IP address: " + WiFi.localIP().toString());
        } else {
            Serial.println("WiFi disconnected!");
        }

        // Notify callback about connection change
        if (_connectionCallback) {
            _connectionCallback(_isConnected);
        }
    }
}

void CustomWiFiManager::onWiFiEvent(WiFiEvent_t event) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            Serial.println("WiFi STA Connected");
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.println("WiFi STA Got IP: " + WiFi.localIP().toString());
            _isConnected = true;
            _isConnecting = false;
            if (_connectionCallback) {
                _connectionCallback(true);
            }
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.println("WiFi STA Disconnected");
            _isConnected = false;
            _isConnecting = false;
            if (_connectionCallback) {
                _connectionCallback(false);
            }
            // Only attempt reconnection if we have credentials and aren't in config mode
            if (_credentialsExist && WiFi.getMode() != WIFI_AP) {
                _lastReconnectAttempt = millis();
            }
            break;
        case ARDUINO_EVENT_WIFI_STA_START:
            Serial.println("WiFi STA Started");
            break;
        case ARDUINO_EVENT_WIFI_STA_STOP:
            Serial.println("WiFi STA Stopped");
            break;
        default:
            break;
    }
}

// checkCredentialsExist() removed - autoConnect() handles this automatically

void CustomWiFiManager::restartDevice() {
    Serial.println("Restarting device...");
    delay(1000);
    ESP.restart();
}