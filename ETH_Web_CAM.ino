/**
 * @file      ESP32-S3-ETH_2025-04-01.ino
 * @author    Modified by Manus.im from original by Lewis He (lewishe@outlook.com)
 * @license   MIT
 * @copyright Copyright (c) 2023  Shenzhen Xin Yuan Electronic Technology Co., Ltd
 * @date      2025-04-01
 * @note      Camera functionality removed, only Ethernet and GPIO functionality remains
 */

#include <WiFi.h>
#if ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3,0,0)
#include <ETHClass2.h>       //Is to use the modified ETHClass
#define ETH  ETH2
#else
#include <ETH.h>
#endif
#include <EEPROM.h>
#include "utilities.h"          //Board PinMap
#include "network_config.h"
#include "neopixel.h"

static bool eth_connected = false;

void WiFiEvent(arduino_event_id_t event)
{
    switch (event) {
    case ARDUINO_EVENT_ETH_START:
        Serial.println("ETH Started");
        //set eth hostname here
        ETH.setHostname("esp32-ethernet");
        // Configure network settings
        if (networkConfig.dhcp_enabled) {
            Serial.println("Using DHCP");
        } else {
            Serial.println("Using static IP");
            ETH.config(
                IPAddress(networkConfig.ip[0], networkConfig.ip[1], networkConfig.ip[2], networkConfig.ip[3]),
                IPAddress(networkConfig.gateway[0], networkConfig.gateway[1], networkConfig.gateway[2], networkConfig.gateway[3]),
                IPAddress(networkConfig.subnet[0], networkConfig.subnet[1], networkConfig.subnet[2], networkConfig.subnet[3]),
                IPAddress(networkConfig.dns1[0], networkConfig.dns1[1], networkConfig.dns1[2], networkConfig.dns1[3]),
                IPAddress(networkConfig.dns2[0], networkConfig.dns2[1], networkConfig.dns2[2], networkConfig.dns2[3])
            );
        }
        break;
    case ARDUINO_EVENT_ETH_CONNECTED:
        Serial.println("ETH Connected");
        break;
    case ARDUINO_EVENT_ETH_GOT_IP:
        Serial.print("ETH MAC: ");
        Serial.print(ETH.macAddress());
        Serial.print(", IPv4: ");
        Serial.print(ETH.localIP());
        if (ETH.fullDuplex()) {
            Serial.print(", FULL_DUPLEX");
        }
        Serial.print(", ");
        Serial.print(ETH.linkSpeed());
        Serial.print("Mbps");
        Serial.print(", ");
        Serial.print("GatewayIP:");
        Serial.println(ETH.gatewayIP());
        eth_connected = true;
        break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
        Serial.println("ETH Disconnected");
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

void startWebServer();

void setup()
{
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    Serial.println();
    
    // Initialize EEPROM for network configuration
    EEPROM.begin(512);
    
    // Initialize network configuration
    initNetworkConfig();
    loadNetworkConfig();
    
    // Initialize NeoPixel
    initNeoPixel();
    
    // Set NeoPixel to blue during initialization
    setNeoPixelColor(0, 0, 255);
    
    WiFi.onEvent(WiFiEvent);

#ifdef ETH_POWER_PIN
    pinMode(ETH_POWER_PIN, OUTPUT);
    digitalWrite(ETH_POWER_PIN, HIGH);
#endif

#if CONFIG_IDF_TARGET_ESP32
    if (!ETH.begin(ETH_TYPE, ETH_ADDR, ETH_MDC_PIN,
                   ETH_MDIO_PIN, ETH_RESET_PIN, ETH_CLK_MODE)) {
        Serial.println("ETH start Failed!");
    }
#else
    if (!ETH.begin(ETH_PHY_W5500, 1, ETH_CS_PIN, ETH_INT_PIN, ETH_RST_PIN,
                   SPI3_HOST,
                   ETH_SCLK_PIN, ETH_MISO_PIN, ETH_MOSI_PIN)) {
        Serial.println("ETH start Failed!");
    }
#endif

    while (!eth_connected) {
        Serial.println("Wait ETH Connect...");
        delay(1000);
    };

    // Set NeoPixel to green when connected
    setNeoPixelColor(0, 255, 0);
    
    // Start the web server
    startWebServer();
    
    Serial.println("Setup complete!");
}

void loop()
{
    // Do nothing. Everything is done in another task by the web server
    delay(10000);
}
