#ifndef CUSTOM_WIFI_MANAGER_H
#define CUSTOM_WIFI_MANAGER_H

#include <WiFi.h>
#include <WiFiManager.h> // tzapu WiFiManager library
#include <Button2.h>
#include <esp_system.h>

class CustomWiFiManager {
public:
    CustomWiFiManager();

    void begin();
    void loop();

    bool isConnected() const;
    bool isConnecting() const;

    typedef void (*ConnectionCallback)(bool connected);
    void setConnectionCallback(ConnectionCallback callback);

private:
    static const int BUTTON_PIN = D1;
    static const unsigned long BUTTON_HOLD_TIME = 3000; // 3 seconds
    static const unsigned long RECONNECT_INTERVAL = 30000; // 30 seconds
    static const unsigned long CONNECTION_TIMEOUT = 15000; // 15 seconds

    ::WiFiManager wifiManager;
    Button2 configButton;

    bool _isConnected;
    bool _isConnecting;
    bool _credentialsExist;
    unsigned long _lastReconnectAttempt;
    unsigned long _connectionStartTime;

    ConnectionCallback _connectionCallback;

    void setupButton();
    void setupWiFiManager();

    void handleButtonHold(Button2& btn);
    void startConfigPortal();
    void tryConnectToWiFi();
    void handleReconnection();
    void checkConnection();
    void onWiFiEvent(WiFiEvent_t event);

    void restartDevice();

    static void wifiEventHandler(WiFiEvent_t event);
    static CustomWiFiManager* instance;
};

#endif // CUSTOM_WIFI_MANAGER_H