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
NetworkConfig networkConfig;

// EEPROM address for network configuration
const int NETWORK_CONFIG_ADDR = 0;
const uint32_t NETWORK_CONFIG_MAGIC = 0x4E455443; // "NETC"

// Initialize network configuration with default values
void initNetworkConfig() {
    // Default to static IP
    networkConfig.dhcp_enabled = false;
    
    // Default IP: 192.168.178.65
    networkConfig.ip[0] = 192;
    networkConfig.ip[1] = 168;
    networkConfig.ip[2] = 178;
    networkConfig.ip[3] = 65;
    
    // Default gateway: 192.168.178.1
    networkConfig.gateway[0] = 192;
    networkConfig.gateway[1] = 168;
    networkConfig.gateway[2] = 178;
    networkConfig.gateway[3] = 1;
    
    // Default subnet: 255.255.255.0
    networkConfig.subnet[0] = 255;
    networkConfig.subnet[1] = 255;
    networkConfig.subnet[2] = 255;
    networkConfig.subnet[3] = 0;
    
    // Default DNS1: 8.8.8.8 (Google DNS)
    networkConfig.dns1[0] = 8;
    networkConfig.dns1[1] = 8;
    networkConfig.dns1[2] = 8;
    networkConfig.dns1[3] = 8;
    
    // Default DNS2: 8.8.4.4 (Google DNS)
    networkConfig.dns2[0] = 8;
    networkConfig.dns2[1] = 8;
    networkConfig.dns2[2] = 4;
    networkConfig.dns2[3] = 4;
    
    // Default hostname
    strcpy(networkConfig.hostname, "esp32-ethernet");
}

// Save network configuration to EEPROM
void saveNetworkConfig() {
    uint32_t magic = NETWORK_CONFIG_MAGIC;
    
    // Write magic number
    EEPROM.put(NETWORK_CONFIG_ADDR, magic);
    
    // Write network configuration
    EEPROM.put(NETWORK_CONFIG_ADDR + sizeof(magic), networkConfig);
    
    // Commit changes
    EEPROM.commit();
    
    Serial.println("Network configuration saved");
}

// Load network configuration from EEPROM
bool loadNetworkConfig() {
    uint32_t magic;
    
    // Read magic number
    EEPROM.get(NETWORK_CONFIG_ADDR, magic);
    
    // Check if magic number matches
    if (magic == NETWORK_CONFIG_MAGIC) {
        // Read network configuration
        EEPROM.get(NETWORK_CONFIG_ADDR + sizeof(magic), networkConfig);
        Serial.println("Network configuration loaded");
        return true;
    } else {
        // No valid configuration found, use defaults
        Serial.println("No valid network configuration found, using defaults");
        initNetworkConfig();
        saveNetworkConfig();
        return false;
    }
}

// Check if an IP address is valid
bool isValidIP(const char* ip) {
    if (ip == NULL) {
        return false;
    }
    
    // Count dots
    int dots = 0;
    for (int i = 0; ip[i] != '\0'; i++) {
        if (ip[i] == '.') {
            dots++;
        }
    }
    
    // IP address must have exactly 3 dots
    if (dots != 3) {
        return false;
    }
    
    // Check each octet
    int octet = 0;
    int octets = 0;
    for (int i = 0; ip[i] != '\0'; i++) {
        if (ip[i] >= '0' && ip[i] <= '9') {
            octet = octet * 10 + (ip[i] - '0');
            if (octet > 255) {
                return false;
            }
        } else if (ip[i] == '.') {
            octets++;
            octet = 0;
        } else {
            return false;
        }
    }
    
    // Count the last octet
    octets++;
    
    // IP address must have exactly 4 octets
    return (octets == 4);
}

#endif // NETWORK_CONFIG_H
