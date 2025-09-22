# CCABN Firmware Project Context

## Project Overview
CCABN Firmware - PlatformIO-based embedded project for sending and receiving data to and from a client over a websocket and mDNS connection.
The esp32s3 will have a camera module attached to it. A 240x240 pixel grayscale video feed will be sent over the websocket to the client. 
The server (esp32s3) will receive a float value between 0 and 1 that will control how bright the leds on the device are.
It should also be able to receive a command that tells it to update its firmware from the github releases page.
I want the device to be discoverable on the network with mDNS so that if the IP address changes it won't matter.
The esp32s3 will have an led array with brightness controlled by the client, as mentioned before. It will also have a single button and 
a status led. The led will be a stable on when the device is powered and running normally. When it is powered on but not connected to wifi, it should pulse.
The user should never have to plug the device into a computer after flashing firmware for the first time. To set the wifi information
that the device connects to, you will hold down the button for 3 seconds. The status light will immediately turn off, and when the 3 seconds is up, it will start to blink.
This will start an access point from the device called CCABN_TRACKER_X (Where the x is the model number. this value will be set when flashing).
When connecting to the network from any device, it should immediately redirect you to an html page just like the sign in pages on public wifi.
This page should have list of available networks, and you should be able to select the network you want and enter the password for it. Then the esp32 will connect to this 
network from now on. Pressing and holding the button will turn the led off until the 3 seconds is over, then it will go back to a stable light
and exit the access point broadcast mode, returning to the regular system and connecting to wifi to stream the video feed.

## Key Information
- **Platform**: PlatformIO
- **Main Branch**: main
- **Build System**: PlatformIO (migrated from previous system)
- **Recent Changes**: Converted to PlatformIO project structure

## Commands
- **Build**: `pio run`
- **Upload**: `pio run --target upload`
- **Clean**: `pio run --target clean`
- **Monitor**: `pio device monitor`

## Important Notes
- Bootloader compilation issues resolved
- App compilation working
- Fresh start after removing old build system

## Development Guidelines
- Use PlatformIO commands for all build operations
- Test both compilation and upload before major changes
- Follow embedded C/C++ best practices

## Architecture Notes
Using an esp32s3 (xiao seeed studio esp32 s3 sense with camera module)

## Common Issues & Solutions
(Document any recurring issues and their solutions here)

## Dependencies
- **ESPAsyncWebServer + AsyncWebSocket**: WebSocket communication for video stream and LED control
- **ESPmDNS**: Network discovery service
- **esp32-camera**: OV2640 camera module interface
- **WiFiManager (tzapu)**: WiFi configuration with captive portal for AP mode
- **Button2**: Robust button handling library for long-press detection

## Technical Specifications
- **Camera Module**: OV2640
- **Video Format**: Raw grayscale (8-bit per pixel), 240x240 resolution
- **Model Number**: Derived from MAC address (last 3 bytes) for CCABN_TRACKER_X naming
- **Update Method**: OTA firmware updates only when commanded (from GitHub releases)
- **Network Access Point**: CCABN_TRACKER_[MAC] with captive portal for WiFi setup

## WiFi Management System

### Overview
The WiFi management system is implemented in `src/wifi_manager.h` and `src/wifi_manager.cpp` using the `CCabNWiFiManager` class. This system provides robust WiFi connectivity with automatic fallback to configuration mode.

### Key Features
- **Automatic Connection**: On boot, attempts to connect using stored credentials
- **Captive Portal**: Automatic AP mode when no credentials exist
- **Manual Configuration**: 3-second button hold (pin D1) enters config mode
- **Robust Reconnection**: Automatic reconnection attempts every 30 seconds
- **Connection Callbacks**: Notify main application of connection state changes
- **Device Restart**: Restarts after successful credential configuration

### WiFi Behavior Flow
1. **Boot**: Device starts and checks for stored WiFi credentials
2. **Credentials Found**: Attempts connection with 15-second timeout
3. **No Credentials**: Automatically enters AP mode with captive portal
4. **Connection Success**: Notifies main app, enables normal operation
5. **Connection Failure**: If credentials exist, schedules reconnection attempts
6. **Disconnection**: Automatically attempts reconnection, notifies main app
7. **Manual Config**: Button hold for 3 seconds forces config portal

### Configuration Portal
- **AP Name**: `CCABN_TRACKER_[MAC]` (last 6 characters of MAC address)
- **Captive Portal**: Redirects all traffic to configuration page
- **Timeout**: 5-minute portal timeout if no configuration
- **Auto-restart**: Device restarts after successful configuration

### Main Application Integration
```cpp
CCabNWiFiManager wifiManager;

void onWiFiConnectionChange(bool connected) {
    if (connected) {
        // Start main application services
    } else {
        // Stop main application services
    }
}

void setup() {
    wifiManager.setConnectionCallback(onWiFiConnectionChange);
    wifiManager.begin();
}

void loop() {
    wifiManager.loop();

    // Only run main app logic when connected
    if (wifiManager.isConnected()) {
        // Main application code here
    }
}
```

### Important Constraints
- Main application should NOT start until WiFi is connected
- Main application should STOP when WiFi disconnects
- Device never enters AP mode automatically if credentials exist (even if invalid)
- Configuration portal only accessible via button hold or initial setup
- All configuration changes result in device restart

### Development Guidelines
- When debugging WiFi issues, search the internet for solutions
- Test thoroughly with various network conditions
- Ensure robust error handling for all WiFi states
- Monitor connection stability over extended periods