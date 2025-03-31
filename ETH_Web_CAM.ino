/**
 * @file      CameraShield.ino
 * @author    Lewis He (lewishe@outlook.com)
 * @license   MIT
 * @copyright Copyright (c) 2023  Shenzhen Xin Yuan Electronic Technology Co., Ltd
 * @date      2023-08-07 (Updated with static IP and fixes 2024-MM-DD)
 * @note      Only support T-ETH-Lite-ESP32S3, need external camera shield to combine.
 *            This version includes static IP configuration.
 * @Steps
 *              1. Don’t plug in the camera, flash sketch, and plug in the network cable.
 *              2. After startup, check the Serial monitor for Ethernet connection status.
 *                 If using DHCP initially, record the assigned IP. If using static IP, ensure it's configured correctly below.
 *              3. Turn off the power and plug in the camera module.
 *              4. After powering on, open a browser on a computer on the same LAN and enter the URL (either the recorded DHCP address or the configured static IP) to access the camera stream.
 */

#include "esp_camera.h"
#include <WiFi.h> // Used for WiFi event system, even with Ethernet

// Select the correct ETH library based on ESP-Arduino version
#if ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3,0,0)
#include <ETHClass2.h> // Use the modified ETHClass for older versions
#define ETH ETH2
#else
#include <ETH.h>       // Use the standard ETH class for newer versions
#endif

#include "utilities.h" // Contains board-specific pin definitions like IR_FILTER_NUM, ETH pins, etc.

// == CAMERA SHIELD PIN MAPPING ==
// NOTE: Verify these pins match your specific camera shield hardware!
#define PWDN_GPIO_NUM  -1 // Power Down pin (-1 if not used or controlled externally)
#define RESET_GPIO_NUM -1 // Reset pin (-1 if not used)
#define XCLK_GPIO_NUM  3  // Camera clock input
#define SIOD_GPIO_NUM  48 // SCCB (I2C) Data
#define SIOC_GPIO_NUM  47 // SCCB (I2C) Clock

#define Y9_GPIO_NUM    18 // Camera Data 8
#define Y8_GPIO_NUM    15 // Camera Data 7
#define Y7_GPIO_NUM    38 // Camera Data 6
#define Y6_GPIO_NUM    40 // Camera Data 5
#define Y5_GPIO_NUM    42 // Camera Data 4
#define Y4_GPIO_NUM    46 // Camera Data 3
#define Y3_GPIO_NUM    45 // Camera Data 2
#define Y2_GPIO_NUM    41 // Camera Data 1 (LSB)
#define VSYNC_GPIO_NUM 1  // Vertical Sync
#define HREF_GPIO_NUM  2  // Horizontal Reference
#define PCLK_GPIO_NUM  39 // Pixel Clock

// == CAMERA SHIELD POWER ENABLE PIN ==
// This pin might be used to enable/disable power to the camera shield.
// Check your shield's schematic. Set to -1 if not used.
#define CAM_ENABLE_GPIO_NUM 8 // GPIO8 is often used for this on dev boards

// Forward declaration for the function that starts the web server
// (Implementation is usually part of the esp32-camera library examples)
void startCameraServer();

// Global flag to track Ethernet connection status
static bool eth_connected = false;


// == Helper Structure and Array for Frame Size Names ==
// Moved definition BEFORE setup()
// Needs to be kept in sync with framesize_t enum in esp_camera_types.h
typedef struct {
    framesize_t id;
    const char *name;
    uint16_t width;
    uint16_t height;
} framesize_map_t;

// Moved definition BEFORE setup()
static const framesize_map_t framesizeMapping[] = {
  { FRAMESIZE_96X96,    "96x96",    96,   96 },
  { FRAMESIZE_QQVGA,    "QQVGA",    160,  120 },
  { FRAMESIZE_QCIF,     "QCIF",     176,  144 },
  { FRAMESIZE_HQVGA,    "HQVGA",    240,  176 },
  { FRAMESIZE_240X240,  "240x240",  240,  240 },
  { FRAMESIZE_QVGA,     "QVGA",     320,  240 },
  { FRAMESIZE_CIF,      "CIF",      352,  288 }, // Often 400x296 practical
  { FRAMESIZE_VGA,      "VGA",      640,  480 },
  { FRAMESIZE_SVGA,     "SVGA",     800,  600 },
  { FRAMESIZE_XGA,      "XGA",      1024, 768 },
  { FRAMESIZE_SXGA,     "SXGA",     1280, 1024 },
  { FRAMESIZE_UXGA,     "UXGA",     1600, 1200 },
  // Add newer/larger sizes if supported by sensor and ESP-IDF version
};

// Helper function to find frame size info (optional but keeps code cleaner)
const framesize_map_t * findFramesize(framesize_t id) {
    for (size_t i = 0; i < sizeof(framesizeMapping) / sizeof(framesizeMapping[0]); ++i) {
        if (framesizeMapping[i].id == id) {
            return &framesizeMapping[i];
        }
    }
    return nullptr; // Not found
}


// == ETHERNET EVENT HANDLER ==
// This function is called by the networking stack when Ethernet events occur.
void WiFiEvent(arduino_event_id_t event)
{
    switch (event) {
    case ARDUINO_EVENT_ETH_START:
        Serial.println("ETH Started");
        // Set Hostname for easier identification on the network
        ETH.setHostname("esp32-cam-ethernet");
        break;
    case ARDUINO_EVENT_ETH_CONNECTED:
        Serial.println("ETH Connected - Link Up");
        break;
    case ARDUINO_EVENT_ETH_GOT_IP:
        Serial.print("ETH MAC: ");
        Serial.print(ETH.macAddress());
        Serial.print(", IPv4: ");
        Serial.print(ETH.localIP()); // Print the assigned or static IP
        if (ETH.fullDuplex()) {
            Serial.print(", FULL_DUPLEX");
        } else {
             Serial.print(", HALF_DUPLEX");
        }
        Serial.print(", ");
        Serial.print(ETH.linkSpeed());
        Serial.println(" Mbps");
        Serial.print("Gateway IP: ");
        Serial.println(ETH.gatewayIP());
        Serial.print("Subnet Mask: ");
        Serial.println(ETH.subnetMask());
        eth_connected = true; // Set flag now that we have an IP
        break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
        Serial.println("ETH Disconnected - Link Down");
        eth_connected = false;
        break;
    case ARDUINO_EVENT_ETH_STOP:
        Serial.println("ETH Stopped");
        eth_connected = false;
        break;
    default:
        // Handle other WiFi events if needed, though we focus on ETH here
        break;
    }
}

// == SETUP FUNCTION ==
// Initializes hardware and software components.
void setup()
{
    Serial.begin(115200);
    // Serial.setDebugOutput(true); // Uncomment for more verbose debug output
    Serial.println("\n\nStarting T-ETH-Lite-S3 Camera Shield Sketch...");

    // == CAMERA POWER ENABLE ==
    // If your shield has a power enable pin controlled by the ESP32, configure it.
    #if CAM_ENABLE_GPIO_NUM != -1
        pinMode(CAM_ENABLE_GPIO_NUM, OUTPUT);
        // Ensure camera power is OFF initially, following the instructions.
        // Check if HIGH or LOW enables power for your specific shield. Assuming LOW=OFF here.
        digitalWrite(CAM_ENABLE_GPIO_NUM, LOW);
        Serial.println("Camera Power Control Initialized (OFF)");
        delay(100); // Small delay after power state change
    #endif

    // Register the Ethernet event handler function
    WiFi.onEvent(WiFiEvent);

    // == ETHERNET PHY POWER (Optional) ==
    // Some boards require enabling power to the Ethernet PHY chip.
    #ifdef ETH_POWER_PIN
        Serial.printf("Enabling ETH PHY Power on Pin: %d\n", ETH_POWER_PIN);
        pinMode(ETH_POWER_PIN, OUTPUT);
        digitalWrite(ETH_POWER_PIN, HIGH); // Typically HIGH enables power
        delay(500); // Allow PHY power to stabilize
    #endif

    // == INITIALIZE ETHERNET ==
    // Use SPI3 pins defined in utilities.h (or board variant) for W5500
    Serial.println("Initializing Ethernet using W5500...");
    // The ETH.begin call below uses default pins for SPI3 if not specified otherwise in utilities.h
    // Make sure ETH_CS_PIN, ETH_INT_PIN, ETH_RST_PIN, ETH_SCLK_PIN, ETH_MISO_PIN, ETH_MOSI_PIN
    // are correctly defined for your T-ETH-Lite board in the board files or utilities.h.
    // Assuming SPI3_HOST is the correct SPI bus.
    bool eth_init_ok = ETH.begin(ETH_PHY_W5500, 1 /* PHY Address */, ETH_CS_PIN, ETH_INT_PIN, ETH_RST_PIN,
                                 SPI3_HOST, /* SPI Host */
                                 ETH_SCLK_PIN, ETH_MISO_PIN, ETH_MOSI_PIN);

    if (!eth_init_ok) {
        Serial.println("ETH Controller Initialize Failed! Check connections and pin definitions.");
        // Optional: Halt or indicate error permanently (e.g., blink LED)
        while (true) { delay(1000); }
    }

    // =============================================
    // == STATIC IP CONFIGURATION ==
    // Configure static IP settings *before* waiting for connection if you don't want DHCP.
    // If you comment this section out, the board will attempt to get an IP via DHCP.
    // =============================================
    Serial.println("Configuring Static IP Address...");
    IPAddress localIP(10, 140, 8, 65);   // <<< CHANGE THIS TO YOUR DESIRED STATIC IP
    IPAddress gateway(10, 140, 8, 1);    // <<< CHANGE THIS TO YOUR NETWORK GATEWAY
    IPAddress subnet(255, 255, 0, 0);    // <<< CHANGE THIS TO YOUR NETWORK SUBNET MASK
    IPAddress primaryDNS(10, 140, 8, 1); // Optional: Primary DNS Server (often same as gateway)
    IPAddress secondaryDNS(8, 8, 8, 8);  // Optional: Secondary DNS Server (Google's Public DNS)

    if (!ETH.config(localIP, gateway, subnet, primaryDNS, secondaryDNS)) {
        Serial.println("Failed to configure Static IP!");
        // Continue with DHCP or halt depending on requirements
    } else {
         Serial.print("Static IP Configured: ");
         Serial.println(localIP);
    }
    // Note: ETH.config() only sets the configuration. The connection process happens
    // asynchronously and triggers the WiFiEvent handler upon success (getting the IP).


    // == INFRARED FILTER CONTROL PIN ==
    // Configure the pin used to control the camera's IR filter (if applicable).
    // Assumes IR_FILTER_NUM is defined in utilities.h
    #ifdef IR_FILTER_NUM
        pinMode(IR_FILTER_NUM, OUTPUT);
        digitalWrite(IR_FILTER_NUM, LOW); // Set default state (e.g., filter enabled/disabled)
        Serial.println("IR Filter Control Pin Initialized.");
    #endif

    // == WAIT FOR ETHERNET CONNECTION ==
    Serial.println("Waiting for Ethernet connection and IP address...");
    uint32_t wait_start_time = millis();
    while (!eth_connected) {
        Serial.print(".");
        delay(500);
        if (millis() - wait_start_time > 30000) { // 30 second timeout
             Serial.println("\nTimeout waiting for Ethernet connection!");
             // Optional: Halt or retry logic
             // Consider if static IP failed, maybe try DHCP fallback here?
             // For now, just proceed, camera init might still work if link comes up later
             // but web server won't be reachable until connected.
             break;
        }
    }
    Serial.println(); // Newline after waiting dots

    // Only proceed with camera initialization if Ethernet connection seems established
    // (or after timeout if desired)
    // if (!eth_connected) {
    //     Serial.println("Cannot initialize camera without network. Halting.");
    //     while(true) { delay(1000); }
    // }

    Serial.println("Ethernet Connected (or timeout reached). Initializing Camera...");


    // == ENABLE CAMERA POWER (if controlled) ==
    #if CAM_ENABLE_GPIO_NUM != -1
        // Turn camera power ON *before* initializing it.
        // Adjust HIGH/LOW based on your shield's requirements. Assuming HIGH=ON.
        digitalWrite(CAM_ENABLE_GPIO_NUM, HIGH);
        Serial.println("Camera Power Enabled");
        delay(200); // Allow camera power to stabilize before init
    #endif


    // == CAMERA CONFIGURATION ==
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM; // Corrected name (I2C Data)
    config.pin_sccb_scl = SIOC_GPIO_NUM; // Corrected name (I2C Clock)
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000; // 20MHz clock for camera
    config.frame_size = FRAMESIZE_UXGA; // Start with high resolution attempt
    config.pixel_format = PIXFORMAT_JPEG; // JPEG for streaming efficiency
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY; // Grab frame when buffer is free
    config.fb_location = CAMERA_FB_IN_PSRAM; // Use PSRAM for frame buffer
    config.jpeg_quality = 12; // Medium-low quality (0-63, lower is higher quality but larger size)
    config.fb_count = 1;      // Number of frame buffers (1 initially)

    // Adjust settings if PSRAM is available
    if (psramFound()) {
        Serial.println("PSRAM found. Using higher quality settings.");
        config.jpeg_quality = 10; // Better quality
        config.fb_count = 2;      // Use 2 frame buffers for smoother streaming
        config.grab_mode = CAMERA_GRAB_LATEST; // Discard older frames if processing is slow
    } else {
        Serial.println("PSRAM not found. Limiting frame size and using DRAM.");
        // Limit frame size if not enough RAM (UXGA likely too large for DRAM)
        config.frame_size = FRAMESIZE_SVGA; // SVGA (800x600) might work
        // config.frame_size = FRAMESIZE_XGA; // XGA (1024x768) - Check available RAM
        config.fb_location = CAMERA_FB_IN_DRAM; // Use internal DRAM for frame buffer
        config.fb_count = 1; // Only allocate 1 buffer in DRAM to save space
    }

    // Alternative format (often used for image processing like face detection)
    // config.pixel_format = PIXFORMAT_RGB565;
    // if (config.pixel_format != PIXFORMAT_JPEG) {
    //    config.frame_size = FRAMESIZE_240X240; // Smaller resolution for processing
    //    config.fb_count = 2; // Need buffers for processing
    // }


    // == INITIALIZE CAMERA DRIVER ==
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x (%s)\n", err, esp_err_to_name(err)); // Added error name
        Serial.println("Check camera connections, pin mapping, and power. Halting.");
        // Turn off camera power if controlled
        #if CAM_ENABLE_GPIO_NUM != -1
             digitalWrite(CAM_ENABLE_GPIO_NUM, LOW);
        #endif
        // Halt execution
        while (true) { delay(1000); }
        return; // Should not be reached
    }
    Serial.println("Camera Initialized Successfully!");

    // == SENSOR CONFIGURATION ==
    sensor_t *s = esp_camera_sensor_get();
    if (s == NULL) {
        Serial.println("Failed to get camera sensor handle!");
    } else {
         // Print sensor info - **FIXED** to use MIDH and MIDL
        uint16_t manuf_id = (s->id.MIDH << 8) | s->id.MIDL;
        Serial.printf("Camera Sensor Detected: PID=0x%02X VER=0x%02X MID=0x%04X\n", s->id.PID, s->id.VER, manuf_id);

        // Apply sensor-specific settings (like flip, saturation, etc.)
        // Example: OV5640 settings (Check if your sensor is OV5640)
        if (s->id.PID == OV5640_PID) {
             Serial.println("Applying OV5640 specific settings...");
             s->set_vflip(s, 1);       // Flip vertical orientation
             // s->set_hmirror(s, 1);  // Flip horizontal (mirror) if needed
             // s->set_brightness(s, 1); // Adjust brightness (range depends on sensor)
             // s->set_saturation(s, -2); // Adjust saturation (range depends on sensor)
        } else if (s->id.PID == OV2640_PID) {
            Serial.println("Applying OV2640 specific settings...");
            s->set_vflip(s, 0); // Default OV2640 orientation might be correct
             // Add other OV2640 specific settings if needed
        } else {
             Serial.println("Applying generic sensor settings (set_vflip=0)");
             s->set_vflip(s, 0); // Default guess for unknown sensors
        }

        // Set initial frame size for streaming (can be changed via web UI later)
        // Lower resolution usually results in higher FPS initially.
        // Try QVGA (320x240) or CIF (352x288) for faster startup.
        framesize_t initial_frame_size = FRAMESIZE_QVGA;
        if (s->set_framesize(s, initial_frame_size) != 0) {
             // Use helper function to get frame size details - **FIXED** scope issue
             const framesize_map_t *fs_info = findFramesize(initial_frame_size);
             if (fs_info) {
                 Serial.printf("Failed to set initial frame size to %s (%dx%d)\n", fs_info->name, fs_info->width, fs_info->height);
             } else {
                 Serial.printf("Failed to set initial frame size (ID: %d)\n", initial_frame_size);
             }
        } else {
             // Use helper function to get frame size details - **FIXED** scope issue
             const framesize_map_t *fs_info = findFramesize(initial_frame_size);
             if (fs_info) {
                 Serial.printf("Initial frame size set to %s (%dx%d) for streaming.\n", fs_info->name, fs_info->width, fs_info->height);
             } else {
                 Serial.printf("Initial frame size set to ID %d for streaming.\n", initial_frame_size);
             }
        }
    }


    // == START THE CAMERA WEB SERVER ==
    // This function (defined elsewhere, typically in app_httpd.cpp) starts the
    // web server tasks that handle streaming and configuration requests.
    Serial.println("Starting Camera Web Server...");
    startCameraServer();

    Serial.println("Camera Stream Ready!");
    Serial.print("Access the stream via browser at: http://");
    Serial.println(ETH.localIP());
}

// == LOOP FUNCTION ==
// The main loop doesn't need to do much, as the web server and camera capture
// run in separate background tasks (RTOS tasks).
void loop()
{
    // Keep the main task alive, but yield CPU time
    delay(10000); // Check status or perform other low-priority tasks here if needed
}


// Note: You still need the implementation of `startCameraServer()` which usually resides
// in `app_httpd.cpp` from the standard ESP32 Camera Web Server examples. Ensure that
// file is part of your project build.