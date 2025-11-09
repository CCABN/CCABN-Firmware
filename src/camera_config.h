#pragma once

#include "esp_camera.h"

namespace CCABN {

/**
 * @brief Camera pin configuration for custom ESP32-S3 Mini 1U board with OV2640
 *
 * Pin mapping based on custom schematic:
 * - I2C: SDA=IO48, SCL=IO34
 * - Reset: IO33
 * - VSYNC: IO47
 * - HSYNC: IO26
 * - MCLK: IO18
 * - PCLK: IO15
 * - Data pins: D0=IO13, D1=IO10, D2=IO11, D3=IO12, D4=IO14, D5=IO16, D6=IO17, D7=IO21
 */
class CameraConfig {
public:
    static camera_config_t getConfig() {
        camera_config_t config;

        // Pin Configuration for custom ESP32-S3 Mini 1U board
        config.pin_pwdn = -1;      // Power down pin (not connected)
        config.pin_reset = 33;     // Reset pin (CAM_RST)
        config.pin_xclk = 18;      // External clock (CSI_MCLK)
        config.pin_sccb_sda = 48;  // I2C SDA (TWI_SDA)
        config.pin_sccb_scl = 34;  // I2C SCL (TWI_SCK)

        // Data pins (8-bit parallel interface)
        config.pin_d7 = 21;        // CSI_D7
        config.pin_d6 = 17;        // CSI_D6
        config.pin_d5 = 16;        // CSI_D5
        config.pin_d4 = 14;        // CSI_D4
        config.pin_d3 = 12;        // CSI_D3
        config.pin_d2 = 11;        // CSI_D2
        config.pin_d1 = 10;        // CSI_D1
        config.pin_d0 = 13;        // CSI_D0

        // Sync pins
        config.pin_vsync = 47;     // Vertical sync (CSI_VSYNC)
        config.pin_href = 26;      // Horizontal reference (CSI_HSYNC)
        config.pin_pclk = 15;      // Pixel clock (CSI_PCLK)

        // XCLK settings - try lower frequency for stability
        config.xclk_freq_hz = 10000000;  // 10MHz clock frequency (lower for better stability)
        config.ledc_timer = LEDC_TIMER_0;
        config.ledc_channel = LEDC_CHANNEL_0;

        // Image format and quality - use smaller size without PSRAM
        config.pixel_format = PIXFORMAT_JPEG;       // JPEG format for web browser compatibility
        config.frame_size = FRAMESIZE_QVGA;         // QVGA (320x240)
        config.jpeg_quality = 20;                   // 0-63, higher number = lower quality, SMALLER SIZE (important!)
        config.fb_count = 1;                        // Single frame buffer to save memory

        // Memory settings - use DRAM if PSRAM not available
        config.fb_location = CAMERA_FB_IN_DRAM;    // Store frame buffers in DRAM (not PSRAM)
        config.grab_mode = CAMERA_GRAB_LATEST;     // Always grab latest frame

        return config;
    }

    /**
     * @brief Get configuration optimized for testing (lower resolution, higher FPS)
     */
    static camera_config_t getTestConfig() {
        camera_config_t config = getConfig();
        config.frame_size = FRAMESIZE_QQVGA;  // 160x120 for testing
        config.fb_count = 1;                  // Single buffer
        return config;
    }

    /**
     * @brief Get configuration for RGB565 color mode (if needed later)
     */
    static camera_config_t getRGB565Config() {
        camera_config_t config = getConfig();
        config.pixel_format = PIXFORMAT_RGB565;  // 16-bit color
        return config;
    }

    /**
     * @brief Get configuration for JPEG mode (if needed for compression)
     */
    static camera_config_t getJPEGConfig() {
        camera_config_t config = getConfig();
        config.pixel_format = PIXFORMAT_JPEG;
        config.jpeg_quality = 10;  // Lower number = higher quality
        return config;
    }
};

} // namespace CCABN