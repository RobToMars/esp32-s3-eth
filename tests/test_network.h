#ifndef TEST_NETWORK_H
#define TEST_NETWORK_H

#include <AUnit.h>
#include "test_common.h"
#include "network_config_wrapper.h"
#include "test_mock_implementations.h"

// Test network configuration initialization
test(NetworkConfigInitialization) {
  // Test initializing network configuration
  initNetworkConfig();
  
  // Verify default values
  assertEqual(networkConfig.dhcp_enabled, false);
  assertEqual(networkConfig.ip[0], 192);
  assertEqual(networkConfig.ip[1], 168);
  assertEqual(networkConfig.ip[2], 178);
  assertEqual(networkConfig.ip[3], 65);
  
  assertEqual(networkConfig.gateway[0], 192);
  assertEqual(networkConfig.gateway[1], 168);
  assertEqual(networkConfig.gateway[2], 178);
  assertEqual(networkConfig.gateway[3], 1);
  
  assertEqual(networkConfig.subnet[0], 255);
  assertEqual(networkConfig.subnet[1], 255);
  assertEqual(networkConfig.subnet[2], 255);
  assertEqual(networkConfig.subnet[3], 0);
  
  assertEqual(networkConfig.dns1[0], 8);
  assertEqual(networkConfig.dns1[1], 8);
  assertEqual(networkConfig.dns1[2], 8);
  assertEqual(networkConfig.dns1[3], 8);
  
  assertEqual(networkConfig.dns2[0], 8);
  assertEqual(networkConfig.dns2[1], 8);
  assertEqual(networkConfig.dns2[2], 4);
  assertEqual(networkConfig.dns2[3], 4);
  
  assertEqual(strcmp(networkConfig.hostname, "esp32-ethernet"), 0);
}

// Test static IP configuration
test(NetworkConfigStaticIP) {
  // Test setting static IP
  networkConfig.dhcp_enabled = false;
  networkConfig.ip[0] = 192;
  networkConfig.ip[1] = 168;
  networkConfig.ip[2] = 1;
  networkConfig.ip[3] = 100;
  
  networkConfig.gateway[0] = 192;
  networkConfig.gateway[1] = 168;
  networkConfig.gateway[2] = 1;
  networkConfig.gateway[3] = 1;
  
  // Save configuration
  saveNetworkConfig();
  
  // Load configuration
  loadNetworkConfig();
  
  // Verify values
  assertEqual(networkConfig.dhcp_enabled, false);
  assertEqual(networkConfig.ip[0], 192);
  assertEqual(networkConfig.ip[1], 168);
  assertEqual(networkConfig.ip[2], 1);
  assertEqual(networkConfig.ip[3], 100);
  
  assertEqual(networkConfig.gateway[0], 192);
  assertEqual(networkConfig.gateway[1], 168);
  assertEqual(networkConfig.gateway[2], 1);
  assertEqual(networkConfig.gateway[3], 1);
}

// Test DHCP configuration
test(NetworkConfigDHCP) {
  // Test setting DHCP
  networkConfig.dhcp_enabled = true;
  
  // Save configuration
  saveNetworkConfig();
  
  // Load configuration
  loadNetworkConfig();
  
  // Verify values
  assertEqual(networkConfig.dhcp_enabled, true);
}

// Test IP validation
test(NetworkConfigIPValidation) {
  // Test valid IP addresses
  assertTrue(isValidIP("192.168.1.1"));
  assertTrue(isValidIP("10.0.0.1"));
  assertTrue(isValidIP("172.16.0.1"));
  assertTrue(isValidIP("255.255.255.255"));
  
  // Test invalid IP addresses
  assertFalse(isValidIP("192.168.1"));       // Missing octet
  assertFalse(isValidIP("192.168.1.256"));   // Octet > 255
  assertFalse(isValidIP("192.168.1.1.1"));   // Too many octets
  assertFalse(isValidIP("192.168.1.a"));     // Non-numeric
  assertFalse(isValidIP(""));                // Empty
  assertFalse(isValidIP(NULL));              // NULL
}

#endif // TEST_NETWORK_H
