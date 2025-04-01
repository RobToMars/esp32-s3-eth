# ESP32-S3-ETH GPIO and Network Control

This project transforms the Waveshare ESP32-S3-ETH board into a versatile GPIO and network control system with a web interface. It provides functionality for controlling digital outputs, reading analog inputs, setting PWM outputs, controlling the onboard NeoPixel LED, and configuring network settings.

## Hardware Requirements

- Waveshare ESP32-S3-ETH board
- USB-C cable for power/programming
- Ethernet cable for network connection (optional if using Wi-Fi)
- Power via USB-C or PoE (IEEE 802.3af)

## Software Setup Instructions

### Arduino IDE Configuration
1. Add ESP32 board support URL: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
2. Install ESP32 board package (minimum 3.2.0)
3. Install "Adafruit NeoPixel" library

### Board Configuration
- Board: "ESP32S3 Dev Module"
- Flash Mode: "QIO"
- Partition Scheme: "Huge APP (3MB No OTA)"
- PSRAM: "OPI PSRAM"

### Arduino CLI Commands
```bash
arduino-cli config init
arduino-cli core update-index --additional-urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core install esp32:esp32 --additional-urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli lib install "Adafruit NeoPixel"
arduino-cli compile --fqbn esp32:esp32:esp32s3 ./ESP32-S3-ETH_No_Camera.ino
arduino-cli upload -p [PORT] --fqbn esp32:esp32:esp32s3 ./ESP32-S3-ETH_No_Camera.ino
```

## Features

### GPIO Control
- Digital output control for individual pins
- Multiple digital outputs control
- Analog input reading
- PWM (analog output) control

### NeoPixel Control
- RGB color setting
- Turn off functionality

### Network Configuration
- DHCP or static IP configuration
- View current network settings
- Apply network changes

## Web Interface

The project includes a responsive web interface that allows you to:

1. Control GPIO pins (digital output, analog input, PWM)
2. Set NeoPixel colors
3. View and configure network settings

Access the web interface by navigating to the board's IP address in a web browser.

## API Endpoints

| Category | Endpoint | Example |
|----------|----------|---------|
| **GPIO** | `/gpio/do?pin=16&state=high` | Set pin 16 HIGH |
| **GPIO** | `/gpio/do/all?pins=16,17,18&states=high,low,high` | Set multiple pins |
| **GPIO** | `/gpio/ai/read?pin=1` | Read analog value from pin 1 |
| **GPIO** | `/gpio/ao/set?pin=16&value=128` | Set PWM value (0-255) on pin 16 |
| **GPIO** | `/gpio/overview` | Get overview of all GPIO pins |
| **NeoPixel** | `/neopixel/set?r=255&g=0&b=0` | Red LED |
| **NeoPixel** | `/neopixel/off` | Turn off LED |
| **Network** | `/network/config/get` | Retrieve current settings |
| **Network** | `/network/config/set?dhcp=false&ip=192.168.1.100` | Set static IP |
| **System** | `/restart` | Restart the device |

## Safe GPIO Pins
- **Digital I/O:** 4, 5, 6, 7, 15-21, 35-42, 45, 46
- **Analog Input:** 1-10

## Project Structure
- Main sketch: ESP32-S3-ETH_No_Camera.ino
- Web server: app_httpd.cpp
- Network configuration: network_config.h
- NeoPixel control: neopixel.h
- Pin definitions: utilities.h

## Troubleshooting Tips
- **No Web Interface:** Check IP in Serial Monitor (115200 baud)
- **GPIO Errors:** Avoid pins used by Ethernet (GPIO9-14)
- **Compilation Issues:** Update ESP32 Arduino Core to ≥3.2.0

## Changes from Original Project
This project is a modified version of the ESP32-S3-ETH Web Camera project with all camera-related functionality removed. The following changes were made:

1. Removed all camera-related code and libraries
2. Simplified the web interface to focus on GPIO and network control
3. Enhanced the GPIO control functionality
4. Added comprehensive network configuration options
5. Improved NeoPixel control

## Recovery Prompt
"I need help with the ESP32-S3-ETH GPIO and Network Control project. This is a modified version of the ESP32-S3-ETH Web Camera project with all camera functionality removed. It uses a Waveshare ESP32-S3-ETH board to provide GPIO control (digital output, analog input, PWM), NeoPixel LED control, and network configuration through a web interface. Please help me understand and modify this project."
