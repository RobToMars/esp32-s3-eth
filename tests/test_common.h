#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <AUnit.h>

// Common mock functions shared across test files

// Mock functions for hardware interaction
int mockDigitalRead(int pin) {
  // Return HIGH for even pins, LOW for odd pins in test mode
  return (pin % 2 == 0) ? HIGH : LOW;
}

void mockDigitalWrite(int pin, int value) {
  // In test mode, just record the value was set
}

int mockAnalogRead(int pin) {
  // Return pin * 100 as a mock analog value in test mode
  return pin * 100;
}

void mockLedcAttach(int pin, int freq, int resolution) {
  // In test mode, just record the attachment was made
}

void mockLedcWrite(int pin, int value) {
  // In test mode, just record the value was set
}

void mockLedcDetach(int pin) {
  // In test mode, just record the detachment was made
}

// Mock functions for NeoPixel operations
void mockNeoPixelBegin() {
  // In test mode, just simulate initialization
}

void mockNeoPixelSetPixelColor(uint16_t n, uint8_t r, uint8_t g, uint8_t b) {
  // In test mode, just record the color values
}

void mockNeoPixelShow() {
  // In test mode, just simulate showing pixels
}

// Define a test version of NetworkConfig with dhcp member
struct TestNetworkConfig {
  bool dhcp;
  uint8_t ip[16];
  uint8_t gateway[16];
  uint8_t subnet[16];
};

// Mock functions for network operations
bool mockLoadNetworkConfig(const char* filename, TestNetworkConfig* config) {
  // In test mode, load a predefined test configuration
  config->dhcp = true;
  
  // Use proper string handling for uint8_t arrays
  const char* ipStr = "192.168.1.100";
  const char* gatewayStr = "192.168.1.1";
  const char* subnetStr = "255.255.255.0";
  
  for (int i = 0; i < strlen(ipStr) && i < 16; i++) {
    config->ip[i] = (uint8_t)ipStr[i];
  }
  config->ip[strlen(ipStr) < 16 ? strlen(ipStr) : 15] = 0;
  
  for (int i = 0; i < strlen(gatewayStr) && i < 16; i++) {
    config->gateway[i] = (uint8_t)gatewayStr[i];
  }
  config->gateway[strlen(gatewayStr) < 16 ? strlen(gatewayStr) : 15] = 0;
  
  for (int i = 0; i < strlen(subnetStr) && i < 16; i++) {
    config->subnet[i] = (uint8_t)subnetStr[i];
  }
  config->subnet[strlen(subnetStr) < 16 ? strlen(subnetStr) : 15] = 0;
  
  return true;
}

bool mockSaveNetworkConfig(const char* filename, TestNetworkConfig* config) {
  // In test mode, just return success
  return true;
}

#endif // TEST_COMMON_H
