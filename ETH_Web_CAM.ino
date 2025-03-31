/**
 * @file      CameraShield.ino
 * @author    Lewis He (lewishe@outlook.com)
 * @license   MIT
 * @copyright Copyright (c) 2023  Shenzhen Xin Yuan Electronic Technology Co., Ltd
 * @date      2023-08-07 (Updated with static IP, stream consolidation, and fixes 2024-MM-DD)
 * @note      Only support T-ETH-Lite-ESP32S3, need external camera shield to combine.
 *            Web UI on Port 80, Stream on Port 80 /stream path.
 * @Steps     [Standard steps]
 */

#include "esp_camera.h"
#include <WiFi.h>

#if ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3,0,0)
#include <ETHClass2.h>
#define ETH ETH2
#else
#include <ETH.h>
#endif

#include "utilities.h"

// == CAMERA SHIELD PIN MAPPING ==
// *** VERIFY THESE MATCH YOUR WORKING SETUP ***
#define PWDN_GPIO_NUM  -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  3
#define SIOD_GPIO_NUM  48
#define SIOC_GPIO_NUM  47

#define Y9_GPIO_NUM    18
#define Y8_GPIO_NUM    15
#define Y7_GPIO_NUM    38
#define Y6_GPIO_NUM    40
#define Y5_GPIO_NUM    42
#define Y4_GPIO_NUM    46
#define Y3_GPIO_NUM    45
#define Y2_GPIO_NUM    41
#define VSYNC_GPIO_NUM 1
#define HREF_GPIO_NUM  2
#define PCLK_GPIO_NUM  39

// --- REMOVED explicit CAM_ENABLE_GPIO_NUM define and logic, ---
// --- assuming original code didn't need it or handled it differently. ---
// --- If your *working* code HAD explicit power control, add it back here ---
// #define CAM_ENABLE_GPIO_NUM 8 // Example if needed

void startCameraServer(); // Forward declaration from app_httpd.cpp

static bool eth_connected = false;

// == Helper Structure and Array for Frame Size Names ==
typedef struct {
    framesize_t id;
    const char *name;
    uint16_t width;
    uint16_t height;
} framesize_map_t;

static const framesize_map_t framesizeMapping[] = {
  { FRAMESIZE_96X96,    "96x96",    96,   96 }, // ... (rest of mappings)
  { FRAMESIZE_QQVGA,    "QQVGA",    160,  120 },
  { FRAMESIZE_QCIF,     "QCIF",     176,  144 },
  { FRAMESIZE_HQVGA,    "HQVGA",    240,  176 },
  { FRAMESIZE_240X240,  "240x240",  240,  240 },
  { FRAMESIZE_QVGA,     "QVGA",     320,  240 },
  { FRAMESIZE_CIF,      "CIF",      352,  288 },
  { FRAMESIZE_VGA,      "VGA",      640,  480 },
  { FRAMESIZE_SVGA,     "SVGA",     800,  600 },
  { FRAMESIZE_XGA,      "XGA",      1024, 768 },
  { FRAMESIZE_SXGA,     "SXGA",     1280, 1024 },
  { FRAMESIZE_UXGA,     "UXGA",     1600, 1200 }
};

const framesize_map_t * findFramesize(framesize_t id) {
    for (size_t i = 0; i < sizeof(framesizeMapping) / sizeof(framesizeMapping[0]); ++i) {
        if (framesizeMapping[i].id == id) {
            return &framesizeMapping[i];
        }
    }
    return nullptr;
}

// == ETHERNET EVENT HANDLER ==
void WiFiEvent(arduino_event_id_t event)
{
    switch (event) {
    case ARDUINO_EVENT_ETH_START:
        Serial.println("ETH Started");
        ETH.setHostname("esp32-cam-ethernet");
        break;
    case ARDUINO_EVENT_ETH_CONNECTED:
        Serial.println("ETH Connected - Link Up");
        break;
    case ARDUINO_EVENT_ETH_GOT_IP:
        Serial.print("ETH MAC: ");
        Serial.print(ETH.macAddress());
        Serial.print(", IPv4: ");
        Serial.print(ETH.localIP());
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
        eth_connected = true;
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
        break;
    }
}

// == SETUP FUNCTION ==
void setup()
{
    Serial.begin(115200);
    Serial.println("\n\nStarting T-ETH-Lite-S3 Camera Shield Sketch...");

    // --- Explicit Camera Power Control REMOVED - Reinstate if needed ---
    /*
    #if CAM_ENABLE_GPIO_NUM != -1
        pinMode(CAM_ENABLE_GPIO_NUM, OUTPUT);
        digitalWrite(CAM_ENABLE_GPIO_NUM, LOW); // Assuming LOW is OFF
        Serial.println("Camera Power Control Initialized (OFF)");
        delay(100);
    #endif
    */

    WiFi.onEvent(WiFiEvent);

    #ifdef ETH_POWER_PIN
        Serial.printf("Enabling ETH PHY Power on Pin: %d\n", ETH_POWER_PIN);
        pinMode(ETH_POWER_PIN, OUTPUT);
        digitalWrite(ETH_POWER_PIN, HIGH);
        delay(500);
    #endif

    Serial.println("Initializing Ethernet using W5500...");
    bool eth_init_ok = ETH.begin(ETH_PHY_W5500, 1, ETH_CS_PIN, ETH_INT_PIN, ETH_RST_PIN,
                                 SPI3_HOST,
                                 ETH_SCLK_PIN, ETH_MISO_PIN, ETH_MOSI_PIN);

    if (!eth_init_ok) {
        Serial.println("ETH Controller Initialize Failed! Halting.");
        while (true) { delay(1000); }
    }

    Serial.println("Configuring Static IP Address...");
    IPAddress localIP(192, 168, 178, 65);   // <<< Your Static IP
    IPAddress gateway(192, 168, 178, 1);    // <<< Your Gateway
    IPAddress subnet(255, 255, 0, 0);       // <<< Your Subnet Mask
    IPAddress primaryDNS(192, 168, 178, 1);
    IPAddress secondaryDNS(8, 8, 8, 8);

    if (!ETH.config(localIP, gateway, subnet, primaryDNS, secondaryDNS)) {
        Serial.println("Failed to configure Static IP!");
    } else {
         Serial.print("Static IP Configured: ");
         Serial.println(localIP);
    }

    #ifdef IR_FILTER_NUM
      if (IR_FILTER_NUM >= 0) { // Check if pin is valid
          pinMode(IR_FILTER_NUM, OUTPUT);
          digitalWrite(IR_FILTER_NUM, LOW);
          Serial.println("IR Filter Control Pin Initialized.");
      }
    #endif

    Serial.println("Waiting for Ethernet connection and IP address...");
    uint32_t wait_start_time = millis();
    while (!eth_connected) {
        Serial.print(".");
        delay(500);
        if (millis() - wait_start_time > 30000) {
             Serial.println("\nTimeout waiting for Ethernet connection!");
             break;
        }
    }
    Serial.println();

    Serial.println("Ethernet Connected (or timeout reached). Initializing Camera...");

    // --- Explicit Camera Power ON REMOVED - Reinstate if needed ---
    /*
    #if CAM_ENABLE_GPIO_NUM != -1
        digitalWrite(CAM_ENABLE_GPIO_NUM, HIGH); // Assuming HIGH is ON
        Serial.println("Camera Power Enabled");
        delay(200);
    #endif
    */

    // == CAMERA CONFIGURATION (Keep as verified for your setup) ==
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
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.frame_size = FRAMESIZE_UXGA;
    config.pixel_format = PIXFORMAT_JPEG;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.jpeg_quality = 12;
    config.fb_count = 1;

    if (psramFound()) {
        Serial.println("PSRAM found. Using higher quality settings.");
        config.jpeg_quality = 10;
        config.fb_count = 2;
        config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
        Serial.println("PSRAM not found. Limiting frame size and using DRAM.");
        config.frame_size = FRAMESIZE_SVGA;
        config.fb_location = CAMERA_FB_IN_DRAM;
        config.fb_count = 1;
    }

    // == INITIALIZE CAMERA DRIVER ==
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x (%s)\n", err, esp_err_to_name(err));
        Serial.println("Check camera connections, pin mapping, and power. Halting.");
        // --- Power Off Removed ---
        /*
        #if CAM_ENABLE_GPIO_NUM != -1
             digitalWrite(CAM_ENABLE_GPIO_NUM, LOW); // Turn off if init failed
        #endif
        */
        while (true) { delay(1000); }
        return;
    }
    Serial.println("Camera Initialized Successfully!");

    // == SENSOR CONFIGURATION ==
    sensor_t *s = esp_camera_sensor_get();
    if (s == NULL) {
        Serial.println("Failed to get camera sensor handle!");
    } else {
        uint16_t manuf_id = (s->id.MIDH << 8) | s->id.MIDL;
        Serial.printf("Camera Sensor Detected: PID=0x%02X VER=0x%02X MID=0x%04X\n", s->id.PID, s->id.VER, manuf_id);

        // Apply sensor-specific settings (V-flip is common)
        if (s->id.PID == OV5640_PID) {
             Serial.println("Applying OV5640 specific settings...");
             s->set_vflip(s, 1);
        } else if (s->id.PID == OV2640_PID) {
            Serial.println("Applying OV2640 specific settings...");
            s->set_vflip(s, 0);
        } else {
             Serial.println("Applying generic sensor settings (set_vflip=0)");
             s->set_vflip(s, 0);
        }

        // Set initial frame size for streaming
        framesize_t initial_frame_size = FRAMESIZE_QVGA;
        if (s->set_framesize(s, initial_frame_size) != 0) {
             const framesize_map_t *fs_info = findFramesize(initial_frame_size);
             if (fs_info) {
                 Serial.printf("Failed to set initial frame size to %s (%dx%d)\n", fs_info->name, fs_info->width, fs_info->height);
             } else {
                 Serial.printf("Failed to set initial frame size (ID: %d)\n", initial_frame_size);
             }
        } else {
             const framesize_map_t *fs_info = findFramesize(initial_frame_size);
             if (fs_info) {
                 Serial.printf("Initial frame size set to %s (%dx%d) for streaming.\n", fs_info->name, fs_info->width, fs_info->height);
             } else {
                 Serial.printf("Initial frame size set to ID %d for streaming.\n", initial_frame_size);
             }
        }
    }

    // == START THE CAMERA WEB SERVER ==
    Serial.println("Starting Camera Web Server...");
    startCameraServer(); // This function is defined in app_httpd.cpp

    Serial.println("Camera Stream Ready!");
    Serial.print("Access the stream via browser at: http://");
    Serial.println(ETH.localIP());
}

// == LOOP FUNCTION ==
void loop()
{
    delay(10000);
}