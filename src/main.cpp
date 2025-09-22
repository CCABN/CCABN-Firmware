#include <Arduino.h>
#include "wifi_manager.h"

CustomWiFiManager wifiManager;
static bool appRunning = false;
static unsigned long lastAppLog = 0;
static const unsigned long APP_LOG_INTERVAL = 2000; // 2 seconds

void onWiFiConnectionChange(bool connected) {
    if (connected && !appRunning) {
        Serial.println("✓ WiFi connected - Starting main application");
        appRunning = true;
    } else if (!connected && appRunning) {
        Serial.println("✗ WiFi disconnected - Stopping main application");
        appRunning = false;
    }
    // Ignore redundant state changes
}

void runMainApplication() {
    unsigned long currentTime = millis();

    if (currentTime - lastAppLog >= APP_LOG_INTERVAL) {
        Serial.println("APP IS RUNNING - WiFi connected, all systems operational");
        lastAppLog = currentTime;
    }
}

void setup() {
    Serial.begin(115200);

    // Wait for serial monitor to connect and stabilize
    while (!Serial) {
        delay(10);
    }

    // Additional delay to ensure serial monitor is fully ready
    delay(3000);

    // Send a few test messages to ensure connection is stable
    Serial.println();
    Serial.println("====================================");
    Serial.println("Serial monitor connected!");
    Serial.println("====================================");
    Serial.println("Starting CCABN Firmware...");
    Serial.println("Initializing WiFi Manager...");

    // Set up WiFi connection callback
    wifiManager.setConnectionCallback(onWiFiConnectionChange);

    // Initialize WiFi manager
    wifiManager.begin();

    Serial.println("Setup complete, entering main loop...");
}

void loop() {
    // Always run WiFi manager loop
    wifiManager.loop();

    // Only run main application when WiFi is connected
    if (appRunning && wifiManager.isConnected()) {
        runMainApplication();
    }

    // Small delay for stability
    delay(10);
}