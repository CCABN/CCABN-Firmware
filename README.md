# CCABN Tracker ESP32-S3 Firmware

ESP32-S3 firmware for the CCABN tracker project. This project uses ESP-IDF as a git submodule and is completely self-contained - no need for separate ESP-IDF installation or CLion plugin.

## Features

- Creates WiFi Access Point named "CCABN_Tracker_1"
- Captive portal redirects all web traffic to a "Hello World" page
- DNS server responds to all queries with the ESP32's IP (192.168.4.1)
- HTTP server serves a simple HTML page
- **Self-contained ESP-IDF**: No external ESP-IDF installation required
- **CMake-based**: Direct CMake integration without plugins

## Hardware Requirements

- Seeed Studio XIAO ESP32-S3 Sense
- Camera module (for future features)
- WiFi antenna

## Quick Start

1. **Clone the project**:
   ```bash
   git clone <your-repo-url>
   cd CCABN-Firmware
   git submodule update --init --recursive
   ```

2. **Build the project**:
   ```bash
   mkdir build
   cd build
   cmake ..
   make
   ```

3. **Flash to ESP32**:
   ```bash
   make flash
   ```

4. **Monitor serial output**:
   ```bash
   make monitor
   ```

5. **Flash and monitor in one step**:
   ```bash
   make flash-monitor
   ```

## Available CMake Targets

- `make` or `make app` - Build the application
- `make flash` - Flash firmware to device (uses `/dev/ttyUSB0` by default on Linux)
- `make monitor` - Start serial monitor
- `make flash-monitor` - Flash and start monitor in one step
- `make menuconfig` - Open ESP-IDF configuration menu
- `make clean-all` - Clean all build files
- `make reinstall-idf-tools` - Force reinstall ESP-IDF tools

## Automatic Setup

The build system automatically:
- Installs ESP-IDF tools on first run
- Sets up the proper environment variables
- Uses the ESP-IDF submodule instead of system installation
- Downloads and installs the ESP32 toolchain and dependencies

## CLion Integration

This project works directly with CLion's CMake support:

1. **Open the project**: File → Open → Select the project folder
2. **CMake will auto-configure**: CLion will detect the CMakeLists.txt and configure automatically
3. **Build**: Use the Build button or Ctrl+F9
4. **Flash**: Run `make flash` in the terminal or create a custom run configuration
5. **Monitor**: Run `make monitor` in the terminal

No ESP-IDF plugin required! The project is self-contained.

## Usage

1. Flash the firmware to your ESP32-S3
2. The device will create a WiFi hotspot named "CCABN_Tracker_1"
3. Connect to the hotspot from any device
4. You should automatically be redirected to a "Hello World" page
5. If not redirected, navigate to http://192.168.4.1

## Monitoring

To see debug output:
```bash
idf.py -p /dev/ttyUSB0 monitor
```

Press `Ctrl+]` to exit the monitor.

## Project Structure

```
CCABN-Firmware/
├── CMakeLists.txt          # Main CMake configuration
├── main/
│   ├── CMakeLists.txt      # Main component CMake
│   └── main.cpp            # Main firmware code
└── sdkconfig.defaults      # ESP-IDF default configuration
```

## Troubleshooting

- **Permission denied on /dev/ttyUSB0:** Add your user to the dialout group:
  ```bash
  sudo usermod -a -G dialout $USER
  ```
  Then log out and log back in.

- **Device not found:** Check that the ESP32 is properly connected and the correct port is specified.

- **Build errors:** Ensure ESP-IDF is properly installed and the IDF_PATH environment variable is set.
