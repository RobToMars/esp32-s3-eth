# ESP32-S3-ETH Web Camera with Enhanced Features

This project extends the Waveshare ESP32-S3-ETH Web Camera with additional features including:

1. **Automatic Streaming** - Stream starts automatically when accessing the web interface
2. **GPIO Control** - Digital and analog I/O control via REST API
3. **Network Configuration** - API for reading and setting network parameters
4. **NeoPixel Control** - Control the onboard RGB LED

## Hardware Requirements

- [Waveshare ESP32-S3-ETH Board](https://www.waveshare.com/wiki/ESP32-S3-ETH)
- OV2640 Camera Module (included with the board)
- Ethernet cable for network connection
- USB-C cable for programming and power

## Installation Requirements

1. **Arduino IDE** (version 2.0 or newer recommended)
2. **Arduino CLI** (for command-line compilation)
3. **ESP32 Board Support Package** (version 2.0.5 or newer)
4. **Required Libraries**:
   - Adafruit NeoPixel (for RGB LED control)

To install the required libraries:
1. Open Arduino IDE
2. Go to Tools > Manage Libraries...
3. Search for "Adafruit NeoPixel" and install it

## Board Setup

1. Connect the camera module to the camera connector on the ESP32-S3-ETH board
2. Connect the Ethernet cable to your network
3. Connect the USB-C cable to your computer

## Compilation and Upload

### Using Arduino IDE

1. Extract the ZIP file
2. Open the project folder in Arduino IDE
3. Select the correct board: Tools > Board > ESP32 Arduino > ESP32S3 Dev Module
4. Set the following board options:
   - USB CDC On Boot: Enabled
   - CPU Frequency: 240MHz
   - USB DFU On Boot: Enabled
   - Flash Mode: QIO 80MHz
   - Flash Size: 16MB
   - Partition Scheme: Huge APP (3MB No OTA/1MB SPIFFS)
   - PSRAM: OPI PSRAM
5. Connect your ESP32-S3-ETH board via USB
6. Click Upload

### Using Arduino CLI

```bash
arduino-cli compile --fqbn esp32:esp32:esp32s3 \
  --build-property "build.partitions=huge_app" \
  --build-property "upload.maximum_size=3145728" \
  --build-property "build.flash_mode=qio" \
  --build-property "build.flash_freq=80m" \
  --build-property "build.flash_size=16MB" \
  --build-property "build.psram_type=opi" \
  --build-property "build.cpu_frequency=240" \
  --build-property "build.usb_cdc_on_boot=1" \
  --build-property "build.usb_dfu_on_boot=1"
```

## Usage

1. After uploading, the device will connect to your network with static IP 192.168.178.65
2. Access the web interface at http://192.168.178.65
3. The camera stream will start automatically
4. Use the web interface to control camera settings and view the stream

## API Endpoints

### Camera Endpoints

| Endpoint | Description |
|----------|-------------|
| `/stream` | Camera video stream |
| `/capture` | Capture a single image |
| `/status` | Get camera status |
| `/control` | Control camera parameters |

### GPIO Control

| Endpoint | Description | Example |
|----------|-------------|---------|
| `/gpio/do?pin=[pin]&state=[high/low]` | Set digital output | `/gpio/do?pin=20&state=high` |
| `/gpio/ai/read?pin=[pin]` | Read analog input | `/gpio/ai/read?pin=0` |
| `/gpio/ao/set?pin=[pin]&value=[0-255]` | Set analog output (PWM) | `/gpio/ao/set?pin=16&value=128` |
| `/gpio/do/all?pins=[pins]&states=[states]` | Control multiple digital outputs | `/gpio/do/all?pins=16,17&states=high,low` |
| `/gpio/overview` | Get overview of all GPIO pins | `/gpio/overview` |

### Network Configuration

| Endpoint | Description | Example |
|----------|-------------|---------|
| `/network/config/get` | Get current network configuration | `/network/config/get` |
| `/network/config/set` | Set network configuration | `/network/config/set?dhcp=false&ip=192.168.178.65&gateway=192.168.178.1&apply=true` |
| `/restart` | Restart the device | `/restart` |

### NeoPixel Control

| Endpoint | Description | Example |
|----------|-------------|---------|
| `/neopixel/set?color=[hex]&brightness=[0-255]` | Set NeoPixel color and brightness | `/neopixel/set?color=FF0000&brightness=128` |
| `/neopixel/off` | Turn off NeoPixel | `/neopixel/off` |

## Safe GPIO Pins

The following GPIO pins are safe to use for digital/analog I/O:
- 0, 4, 5, 6, 7, 16, 17, 19, 20, 21, 33, 34, 35, 36, 37, 43, 44

Pins used by the camera and Ethernet are automatically protected from misuse.

## Analog Input Pins

The following pins can be used for analog input:
- 0, 1, 2, 3, 4, 5, 6, 7, 8, 9

## Network Configuration

By default, the device uses a static IP configuration:
- IP: 192.168.178.65
- Gateway: 192.168.178.1
- Subnet: 255.255.255.0
- DNS1: 8.8.8.8
- DNS2: 8.8.4.4

You can change these settings using the network configuration API. The settings are stored in EEPROM and persist across reboots.

Example to set DHCP mode:
```
http://192.168.178.65/network/config/set?dhcp=true&apply=true
```

Example to set static IP:
```
http://192.168.178.65/network/config/set?dhcp=false&ip=192.168.1.100&gateway=192.168.1.1&subnet=255.255.255.0&apply=true
```

## NeoPixel Control

The onboard NeoPixel RGB LED is connected to GPIO 21. You can control its color and brightness using the NeoPixel API endpoints.

Example to set red color at half brightness:
```
http://192.168.178.65/neopixel/set?color=FF0000&brightness=128
```

Example to turn off the LED:
```
http://192.168.178.65/neopixel/off
```

## Camera Settings

You can control various camera settings through the web interface or via the `/control` API endpoint:

| Parameter | Values | Description |
|-----------|--------|-------------|
| framesize | 0-13 | Resolution (0=QQVGA, 10=UXGA) |
| quality | 0-63 | JPEG quality (0=best, 63=worst) |
| brightness | -2 to 2 | Image brightness |
| contrast | -2 to 2 | Image contrast |
| saturation | -2 to 2 | Image saturation |
| special_effect | 0-6 | Special effects (0=none, 1=negative, etc.) |
| hmirror | 0/1 | Horizontal mirror |
| vflip | 0/1 | Vertical flip |

Example to set resolution to VGA and quality to 10:
```
http://192.168.178.65/control?var=framesize&val=5
http://192.168.178.65/control?var=quality&val=10
```

## Project Structure

- **ETH_Web_CAM_[timestamp].ino**: Main Arduino sketch file
- **app_httpd.cpp**: HTTP server implementation and request handlers
- **camera_index.h**: Web interface HTML (compressed)
- **network_config.h**: Network configuration implementation
- **neopixel.h**: NeoPixel control implementation
- **utilities.h**: Utility functions
- **partitions.csv**: Partition table for ESP32-S3
- **update_zipped_html.py**: Script to update the compressed HTML

## Modifying the Web Interface

If you want to modify the web interface:

1. Edit the CameraWeb.html file
2. Run the update_zipped_html.py script to compress the HTML and update camera_index.h
3. Recompile and upload the project

```bash
python update_zipped_html.py CameraWeb.html camera_index.h
```

## Troubleshooting

1. **Camera not working**: Make sure the camera is properly connected and the ribbon cable is seated correctly.
2. **Network connection issues**: Check your Ethernet cable and network settings.
3. **GPIO control not working**: Verify you're using pins that are safe for GPIO control (see list above).
4. **Compilation errors**: Make sure you have installed all required libraries and are using the correct board settings.
5. **Cannot access web interface**: Verify the IP address by checking your router's DHCP client list or the serial monitor output.

## Advanced Usage

### Custom GPIO Projects

You can use the GPIO API to control external hardware connected to the ESP32-S3-ETH board. For example:

- Connect LEDs to digital output pins
- Connect buttons or sensors to digital input pins
- Connect analog sensors to analog input pins
- Control motors or servos using PWM (analog output)

### Integration with Home Automation

The REST API makes it easy to integrate with home automation systems:

- Use the camera stream in Home Assistant
- Control GPIO pins from Node-RED
- Monitor sensor values from custom dashboards

## License

This project is based on the Waveshare ESP32-S3-ETH Web Camera example and is provided under the same license terms.

## Acknowledgments

- Based on the ESP32 Camera example by Espressif
- Enhanced with additional features for the Waveshare ESP32-S3-ETH board
