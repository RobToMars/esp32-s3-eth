# ESP32-S3-ETH Web Camera

## Overview

This project turns the ESP32-S3-ETH board into a versatile web camera with GPIO control capabilities. It provides a web interface for viewing the camera stream and controlling various features of the board, including GPIO pins and NeoPixel LEDs.

## Features

- **Web Camera Interface**: Access the camera stream from any web browser
- **Camera Controls**: Adjust camera settings like brightness, contrast, and resolution
- **GPIO Control**: Control digital and analog I/O pins through a REST API
- **Network Configuration**: Configure static IP or DHCP through the web interface
- **NeoPixel Control**: Control the onboard RGB LED

## Hardware Requirements

- Waveshare ESP32-S3-ETH board
- OV2640 camera module (connected to the camera interface)
- Optional: Additional components for GPIO control (LEDs, sensors, etc.)

## Installation Requirements

### Arduino IDE Setup

1. Install Arduino IDE (version 1.8.x or 2.x)
2. Add ESP32 board support:
   - Open Arduino IDE
   - Go to File > Preferences
   - Add `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json` to Additional Board Manager URLs
   - Go to Tools > Board > Boards Manager
   - Search for "esp32" and install the latest version (minimum 2.0.5, recommended 3.2.0)
3. Install required libraries:
   - Go to Sketch > Include Library > Manage Libraries
   - Search for and install "Adafruit NeoPixel"

### Arduino CLI Setup (Alternative)

If you prefer using Arduino CLI:

1. Install Arduino CLI following the instructions at https://arduino.github.io/arduino-cli/latest/installation/
2. Add ESP32 board support:
   ```
   arduino-cli config init
   arduino-cli core update-index --additional-urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   arduino-cli core install esp32:esp32 --additional-urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Install required libraries:
   ```
   arduino-cli lib install "Adafruit NeoPixel"
   ```

## Board Setup

1. Connect the ESP32-S3-ETH board to your computer via USB
2. Connect the OV2640 camera module to the camera interface on the board
3. In Arduino IDE, select:
   - Board: "ESP32S3 Dev Module" (under ESP32 Arduino)
   - Flash Mode: "QIO"
   - Flash Size: "8MB (64Mb)"
   - Partition Scheme: "Huge APP (3MB No OTA/1MB SPIFFS)"
   - PSRAM: "OPI PSRAM"
   - Upload Speed: "921600"
   - USB CDC On Boot: "Enabled"
   - USB Mode: "Hardware CDC and JTAG"

## Usage

### Arduino IDE

1. Open the project in Arduino IDE
2. Select the correct board and port
3. Click Upload
4. Once uploaded, open the Serial Monitor to see the IP address of the board
5. Open a web browser and navigate to the IP address

### Arduino CLI

1. Navigate to the project directory
2. Compile and upload the code:
   ```
   arduino-cli compile --fqbn esp32:esp32:esp32s3 ./ETH_Web_CAM_[timestamp]/ETH_Web_CAM_[timestamp].ino
   arduino-cli upload -p [PORT] --fqbn esp32:esp32:esp32s3 ./ETH_Web_CAM_[timestamp]/ETH_Web_CAM_[timestamp].ino
   ```
3. Monitor the serial output to get the IP address:
   ```
   arduino-cli monitor -p [PORT]
   ```
4. Open a web browser and navigate to the IP address

## API Endpoints

### Camera Endpoints

- `/camera/stream` - MJPEG stream of the camera
- `/camera/capture` - Capture a single JPEG image
- `/camera/status` - Get camera settings in JSON format
- `/camera/control?var=[VARIABLE]&val=[VALUE]` - Control camera settings

Camera control variables:
- `framesize` (0-10)
- `quality` (0-63)
- `brightness` (-2 to 2)
- `contrast` (-2 to 2)
- `saturation` (-2 to 2)
- `special_effect` (0-6)
- `hmirror` (0/1)
- `vflip` (0/1)

Example: `/camera/control?var=framesize&val=8`

### GPIO Control Endpoints

- `/gpio/overview` - Get overview of all GPIO pins and their states
- `/gpio/do?pin=[PIN]&state=[STATE]` - Set digital output pin
- `/gpio/do/all?pins=[PINS]&states=[STATES]` - Set multiple digital output pins
- `/gpio/ai/read?pin=[PIN]` - Read analog input pin
- `/gpio/ao/set?pin=[PIN]&value=[VALUE]` - Set analog output (PWM) pin

Examples:
- `/gpio/do?pin=16&state=high` - Set pin 16 to HIGH
- `/gpio/do/all?pins=16,17&states=high,low` - Set pin 16 to HIGH and pin 17 to LOW
- `/gpio/ai/read?pin=1` - Read analog value from pin 1
- `/gpio/ao/set?pin=16&value=128` - Set PWM value 128 (0-255) on pin 16

### Network Configuration Endpoints

- `/network/config/get` - Get current network configuration
- `/network/config/set?dhcp=[BOOL]&ip=[IP]&gateway=[IP]&apply=[BOOL]` - Set network configuration
- `/restart` - Restart the device

Examples:
- `/network/config/get` - Returns current network settings
- `/network/config/set?dhcp=false&ip=192.168.1.100&gateway=192.168.1.1&apply=true` - Set static IP
- `/restart` - Restart the device to apply network changes

### NeoPixel Control Endpoints

- `/neopixel/set?r=[R]&g=[G]&b=[B]` - Set NeoPixel color (RGB values 0-255)
- `/neopixel/off` - Turn off NeoPixel

Examples:
- `/neopixel/set?r=255&g=0&b=0` - Set NeoPixel to red
- `/neopixel/off` - Turn off NeoPixel

## Safe GPIO Pins

The following GPIO pins are safe to use for digital and analog I/O:

- Digital Output/Input: 4, 5, 6, 7, 15, 16, 17, 18, 19, 20, 21, 35, 36, 37, 38, 39, 40, 41, 42, 45, 46
- Analog Input (ADC): 1, 2, 3, 4, 5, 6, 7, 8, 9, 10

Note: Some pins may be used by the board for specific functions. Check the Waveshare ESP32-S3-ETH documentation for details.

## Project Structure

- `ETH_Web_CAM_[timestamp].ino` - Main Arduino sketch file
- `app_httpd.cpp` - HTTP server implementation and request handlers
- `camera_index.h` - HTML interface for the camera web page (compressed)
- `network_config.h` - Network configuration functions
- `neopixel.h` - NeoPixel control functions
- `utilities.h` - Utility functions

## Modifying the Web Interface

The web interface is stored in `CameraWeb.html` and compressed into `camera_index.h`. If you want to modify the web interface:

1. Edit `CameraWeb.html`
2. Run the provided `update_zipped_html.py` script to update the compressed version:
   ```
   python update_zipped_html.py
   ```
3. Recompile and upload the code

## Latest Version Features

The latest version includes:

1. Fixed GPIO overview endpoint to show the current state of inputs/outputs (high/low)
2. Fixed network configuration set endpoint to properly save and apply changes
3. Fixed GPIO do all endpoint to correctly handle multiple pins and states
4. Moved camera endpoints to the `/camera/` path for better organization
5. Optimized code size by removing unnecessary functions
6. Fixed compilation issues for compatibility with ESP32 Arduino core 3.2.0

## Troubleshooting

### Cannot Connect to Web Interface
- Check that the board is powered and connected to the network
- Verify the IP address in the Serial Monitor
- Try pinging the IP address to confirm network connectivity
- Check if your computer is on the same network as the ESP32

### Camera Not Working
- Check that the camera is properly connected to the board
- Try resetting the board
- Verify that the camera model is OV2640

### GPIO Control Issues
- Ensure you're using pins that are safe for GPIO (see Safe GPIO Pins section)
- Check that the pin is not already in use by another function
- Verify the pin number in your request

### Compilation Errors
- Update the ESP32 board package to the latest version
- Ensure all required libraries are installed
- Check that the board settings match the recommended settings

## Advanced Usage

### Custom Camera Settings
```
// Set resolution to VGA
http://[ESP32-IP]/camera/control?var=framesize&val=8

// Set quality to best
http://[ESP32-IP]/camera/control?var=quality&val=10

// Enable mirror mode
http://[ESP32-IP]/camera/control?var=hmirror&val=1
```

### GPIO Control Sequence
```
// Turn on LED on pin 16
http://[ESP32-IP]/gpio/do?pin=16&state=high

// Wait 1 second (in your application)

// Turn off LED on pin 16
http://[ESP32-IP]/gpio/do?pin=16&state=low
```

### Multiple GPIO Control
```
// Turn on LED on pin 16 and turn off LED on pin 17
http://[ESP32-IP]/gpio/do/all?pins=16,17&states=high,low
```

## License

This project is licensed under the MIT License.

## Acknowledgments

- Based on the ESP32 Camera Web Server example by Espressif
- Uses Adafruit NeoPixel library for RGB LED control
- Developed for the Waveshare ESP32-S3-ETH board
