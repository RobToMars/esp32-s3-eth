#ifndef NETWORK_CONFIG_H
#define NETWORK_CONFIG_H

#include <EEPROM.h>
#include <Arduino.h>

// Network configuration structure
struct NetworkConfig {
    bool dhcp_enabled;
    uint8_t ip[4];
    uint8_t gateway[4];
    uint8_t subnet[4];
    uint8_t dns1[4];
    uint8_t dns2[4];
    char hostname[32];
};

// Global network configuration
extern NetworkConfig networkConfig;

// EEPROM address for network configuration
const int NETWORK_CONFIG_ADDR = 0;
const uint32_t NETWORK_CONFIG_MAGIC = 0x4E455443; // "NETC"

// Initialize network configuration with default values
void initNetworkConfig();

// Save network configuration to EEPROM
void saveNetworkConfig();

// Load network configuration from EEPROM
bool loadNetworkConfig();

// Check if an IP address is valid
bool isValidIP(const char* ip);

#endif // NETWORK_CONFIG_H
