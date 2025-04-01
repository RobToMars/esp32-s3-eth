#ifndef TEST_NETWORK_H
#define TEST_NETWORK_H

#include <AUnit.h>
#include "test_common.h"
#include "network_config_wrapper.h"

// Define NETWORK_CONFIG_FILE for tests
#define NETWORK_CONFIG_FILE "/network_config.json"

// Test network configuration initialization
test(NetworkConfigInitialization) {
  // Create a test network configuration
  TestNetworkConfig config;
  
  // Initialize with default values
  config.dhcp = false;
  
  const char* ipStr = "0.0.0.0";
  const char* gatewayStr = "0.0.0.0";
  const char* subnetStr = "0.0.0.0";
  
  for (int i = 0; i < strlen(ipStr) && i < 16; i++) {
    config.ip[i] = (uint8_t)ipStr[i];
  }
  config.ip[strlen(ipStr) < 16 ? strlen(ipStr) : 15] = 0;
  
  for (int i = 0; i < strlen(gatewayStr) && i < 16; i++) {
    config.gateway[i] = (uint8_t)gatewayStr[i];
  }
  config.gateway[strlen(gatewayStr) < 16 ? strlen(gatewayStr) : 15] = 0;
  
  for (int i = 0; i < strlen(subnetStr) && i < 16; i++) {
    config.subnet[i] = (uint8_t)subnetStr[i];
  }
  config.subnet[strlen(subnetStr) < 16 ? strlen(subnetStr) : 15] = 0;
  
  // Load test configuration
  mockLoadNetworkConfig(NETWORK_CONFIG_FILE, &config);
  
  // Verify configuration was loaded
  assertTrue(config.dhcp);
  
  // Convert uint8_t arrays to char arrays for comparison
  char ipStrLoaded[17];
  for (int i = 0; i < 16; i++) {
    ipStrLoaded[i] = (char)config.ip[i];
    if (config.ip[i] == 0) break;
  }
  ipStrLoaded[16] = 0;
  
  // Verify IP address
  assertEqual(strcmp(ipStrLoaded, "192.168.1.100"), 0);
}

// Test setting static IP configuration
test(NetworkConfigStaticIP) {
  // Create a test network configuration
  TestNetworkConfig config;
  
  // Set static IP configuration
  config.dhcp = false;
  
  const char* ipStr = "192.168.1.200";
  const char* gatewayStr = "192.168.1.1";
  const char* subnetStr = "255.255.255.0";
  
  for (int i = 0; i < strlen(ipStr) && i < 16; i++) {
    config.ip[i] = (uint8_t)ipStr[i];
  }
  config.ip[strlen(ipStr) < 16 ? strlen(ipStr) : 15] = 0;
  
  for (int i = 0; i < strlen(gatewayStr) && i < 16; i++) {
    config.gateway[i] = (uint8_t)gatewayStr[i];
  }
  config.gateway[strlen(gatewayStr) < 16 ? strlen(gatewayStr) : 15] = 0;
  
  for (int i = 0; i < strlen(subnetStr) && i < 16; i++) {
    config.subnet[i] = (uint8_t)subnetStr[i];
  }
  config.subnet[strlen(subnetStr) < 16 ? strlen(subnetStr) : 15] = 0;
  
  // Save configuration
  bool result = mockSaveNetworkConfig(NETWORK_CONFIG_FILE, &config);
  
  // Verify save was successful
  assertTrue(result);
  
  // Verify configuration is static
  assertFalse(config.dhcp);
  
  // Convert uint8_t arrays to char arrays for comparison
  char ipStrSaved[17];
  for (int i = 0; i < 16; i++) {
    ipStrSaved[i] = (char)config.ip[i];
    if (config.ip[i] == 0) break;
  }
  ipStrSaved[16] = 0;
  
  // Verify IP address
  assertEqual(strcmp(ipStrSaved, "192.168.1.200"), 0);
}

// Test setting DHCP configuration
test(NetworkConfigDHCP) {
  // Create a test network configuration
  TestNetworkConfig config;
  
  // Set DHCP configuration
  config.dhcp = true;
  
  // Save configuration
  bool result = mockSaveNetworkConfig(NETWORK_CONFIG_FILE, &config);
  
  // Verify save was successful
  assertTrue(result);
  
  // Verify configuration is DHCP
  assertTrue(config.dhcp);
}

// Test IP address validation
test(NetworkConfigIPValidation) {
  // Test valid IP addresses
  assertTrue(isValidIP("192.168.1.1"));
  assertTrue(isValidIP("10.0.0.1"));
  assertTrue(isValidIP("172.16.0.1"));
  
  // Test invalid IP addresses
  assertFalse(isValidIP("256.168.1.1"));  // Value > 255
  assertFalse(isValidIP("192.168.1"));    // Missing octet
  assertFalse(isValidIP("192.168.1.1.1")); // Extra octet
  assertFalse(isValidIP("192.168.1."));   // Trailing dot
  assertFalse(isValidIP("192.168..1"));   // Empty octet
  assertFalse(isValidIP("not an ip"));    // Not an IP at all
}

#endif // TEST_NETWORK_H
