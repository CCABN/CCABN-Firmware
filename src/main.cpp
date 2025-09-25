#include <Arduino.h>
#include <WiFiManager.h>
#include <Button2.h>
#include <WiFi.h>

// Pin definitions
#define BUTTON_PIN D1  // Physical D1 pin (GPIO2)

// Global variables
Button2 button;
String apName;

// Generate AP name with MAC address
String generateAPName() {
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    return "CCABN_TRACKER_" + mac.substring(6); // Last 6 chars
}

// Button callbacks for debugging
void onButtonPressed(Button2& btn) {
    Serial.println("Button pressed!");
}

void onButtonReleased(Button2& btn) {
    Serial.println("Button released!");
}

// Button callback for 3-second hold
void onButtonLongPress(Button2& btn) {
    Serial.println("=== BUTTON HELD FOR 3 SECONDS ===");
    Serial.println("Starting AP configuration mode...");

    WiFiManager wm;

    // Enable debug output
    wm.setDebugOutput(true);

    // Improve network scanning for mobile hotspots
    wm.setMinimumSignalQuality(0);  // Show all networks regardless of signal strength
    wm.setRemoveDuplicateAPs(false); // Don't remove duplicate APs (some hotspots appear multiple times)
    wm.setScanDispPerc(true);       // Show signal strength as percentage

    Serial.println("AP Name: " + apName);
    Serial.println("Configuration portal will run indefinitely until configured");

    // Start config portal (blocks until configured)
    if (!wm.startConfigPortal(apName.c_str())) {
        Serial.println("Failed to start config portal");
    } else {
        Serial.println("Configuration completed successfully!");
    }

    Serial.println("=== RETURNING TO MAIN LOOP ===");
}

void setup() {
    Serial.begin(115200);

    // Wait for serial monitor
    while (!Serial) {
        delay(10);
    }
    delay(2000);

    Serial.println();
    Serial.println("====================================");
    Serial.println("    CCABN Firmware Starting");
    Serial.println("====================================");

    // Generate AP name first (needs WiFi mode set)
    WiFi.mode(WIFI_STA);
    apName = generateAPName();
    Serial.println("Generated AP name: " + apName);

    // Setup button
    button.begin(BUTTON_PIN);
    button.setLongClickTime(3000); // 3 seconds
    button.setLongClickHandler(onButtonLongPress);
    button.setPressedHandler(onButtonPressed);
    button.setReleasedHandler(onButtonReleased);
    Serial.println("Button configured on pin D1 (GPIO2) with debug callbacks");

    // Start AP auto mode
    Serial.println("Starting WiFiManager autoConnect...");

    WiFiManager wm;

    // Enable debug output for library
    wm.setDebugOutput(true);

    // Improve network scanning for mobile hotspots
    wm.setMinimumSignalQuality(0);  // Show all networks regardless of signal strength
    wm.setRemoveDuplicateAPs(false); // Don't remove duplicate APs (some hotspots appear multiple times)
    wm.setScanDispPerc(true);       // Show signal strength as percentage

    // This line not reached until AP mode exits (if it starts)
    bool connected = wm.autoConnect(apName.c_str());

    if (connected) {
        Serial.println("=== WIFI CONNECTED SUCCESSFULLY ===");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        Serial.print("SSID: ");
        Serial.println(WiFi.SSID());
    } else {
        Serial.println("=== WIFI CONNECTION FAILED ===");
        Serial.println("Continuing to main loop...");
    }

    Serial.println("=== SETUP COMPLETE - ENTERING MAIN LOOP ===");
}

void loop() {
    // Handle button events
    button.loop();

    // WiFi status checking (built into library)
    static unsigned long lastStatusCheck = 0;
    static bool lastConnectedState = false;

    if (millis() - lastStatusCheck > 5000) { // Check every 5 seconds
        bool currentlyConnected = (WiFi.status() == WL_CONNECTED);

        if (currentlyConnected != lastConnectedState) {
            if (currentlyConnected) {
                Serial.println("✓ WiFi reconnected!");
                Serial.print("IP: ");
                Serial.println(WiFi.localIP());
            } else {
                Serial.println("✗ WiFi disconnected!");
            }
            lastConnectedState = currentlyConnected;
        }

        // Periodic status update
        Serial.print("WiFi Status: ");
        if (currentlyConnected) {
            Serial.println("Connected to " + WiFi.SSID());
        } else {
            Serial.println("Disconnected");
        }

        lastStatusCheck = millis();
    }

    // Run application loop here
    // (Main application code would go here when WiFi is connected)

    delay(100); // Small delay for stability
}