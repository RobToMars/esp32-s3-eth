## ESP32-S3-ETH Web Camera Project Summary

### Project Overview
This project turns a Waveshare ESP32-S3-ETH board into a web camera with GPIO control capabilities, providing a web interface for viewing the camera stream and controlling various features including GPIO pins and NeoPixel LEDs.

### Hardware Requirements
- Waveshare ESP32-S3-ETH board
- OV2640 camera module
- USB-C cable for power/programming
- Ethernet cable (optional if using Wi-Fi)
- Power via USB-C or PoE (IEEE 802.3af)

### Software Setup Instructions
1. **Arduino IDE Configuration:**
   - Add ESP32 board support URL: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - Install ESP32 board package (minimum 3.2.0)
   - Install "Adafruit NeoPixel" library

2. **Board Configuration:**
   - Board: "ESP32S3 Dev Module"
   - Flash Mode: "QIO"
   - Partition Scheme: "Huge APP (3MB No OTA)"
   - PSRAM: "OPI PSRAM"

3. **Arduino CLI Commands:**
   ```bash
   arduino-cli config init
   arduino-cli core update-index --additional-urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   arduino-cli core install esp32:esp32 --additional-urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   arduino-cli lib install "Adafruit NeoPixel"
   arduino-cli compile --fqbn esp32:esp32:esp32s3 ./ESP32-S3-ETH_2025-04-01.ino
   arduino-cli upload -p [PORT] --fqbn esp32:esp32:esp32s3 ./ESP32-S3-ETH_2025-04-01.ino
   ```

### Key API Endpoints
| Category | Endpoint | Example |
|----------|----------|---------|
| **Camera** | `/camera/control?var=framesize&val=8` | Set resolution to VGA |
| **GPIO** | `/gpio/do?pin=16&state=high` | Set pin 16 HIGH |
| **NeoPixel** | `/neopixel/set?r=255&g=0&b=0` | Red LED |
| **Network** | `/network/config/get` | Retrieve current settings |

### Safe GPIO Pins
- **Digital I/O:** 4, 5, 6, 7, 15-21, 35-42, 45, 46
- **Analog Input:** 1-10

### Test Fixes Implemented
1. Updated `test_mock_implementations.h` to match safe pin definitions in `app_httpd.cpp`
2. Fixed `test_gpio.h` to correctly test pin safety for pins 6 and 7
3. Fixed GpioAnalogInput test to use pin 11 (invalid) instead of pin 6 (valid)
4. Improved GpioMultipleDigitalOutputs test to properly count processed pins
5. Updated `test_network.h` for proper IP validation testing

### Troubleshooting Tips
- **No Web Interface:** Check IP in Serial Monitor (115200 baud)
- **Camera Failure:** Verify OV2640 connection
- **GPIO Errors:** Avoid pins used by Ethernet (GPIO9-14)
- **Compilation Issues:** Update ESP32 Arduino Core to ≥3.2.0

### Project Structure
- Main sketch: ESP32-S3-ETH_2025-04-01.ino
- Web interface: CameraWeb.html (compressed into camera_index.h)
- HTTP server: app_httpd.cpp
- Network configuration: network_config.h
- NeoPixel control: neopixel.h
- Tests: in /tests directory

### Modifying Web Interface
1. Edit CameraWeb.html
2. Run update_zipped_html.py
3. Recompile and upload

### Compilation Stats
- Program storage: 1101278 bytes (84% of 1310720 bytes)
- Dynamic memory: 60800 bytes (18% of 327680 bytes)

### Recovery Prompt
"I need help with the ESP32-S3-ETH Web Camera project from April 1, 2025. The project uses a Waveshare ESP32-S3-ETH board with an OV2640 camera module to create a web camera with GPIO control capabilities. The code includes fixes for test failures in the GPIO and network configuration tests. Please help me understand and modify this project."
