#ifndef TEST_MOCK_IMPLEMENTATIONS_H
#define TEST_MOCK_IMPLEMENTATIONS_H

#include <Arduino.h>

// Mock implementations of functions declared in wrapper headers
// These implementations will be used during testing

// GPIO function implementations
bool is_pin_safe(int pin) {
  // Safe pins for ESP32-S3-ETH (matching app_httpd.cpp)
  const int safe_pins[] = { 4, 5, 6, 7, 15, 16, 17, 18, 19, 20, 21, 35, 36, 37, 38, 39, 40, 41, 42, 45, 46 };
  const int num_safe_pins = sizeof(safe_pins) / sizeof(safe_pins[0]);
  
  for (int i = 0; i < num_safe_pins; i++) {
    if (pin == safe_pins[i]) {
      return true;
    }
  }
  
  return false;
}

bool is_pin_safe_ai(int pin) {
  // Safe analog input pins (matching app_httpd.cpp)
  const int safe_ai_pins[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
  const int num_safe_ai_pins = sizeof(safe_ai_pins) / sizeof(safe_ai_pins[0]);
  
  for (int i = 0; i < num_safe_ai_pins; i++) {
    if (pin == safe_ai_pins[i]) {
      return true;
    }
  }
  
  return false;
}

bool is_pin_safe_ao(int pin) {
  // Safe analog output pins (same as digital pins)
  return is_pin_safe(pin);
}

int initDigitalOutput(int pin) {
  if (!is_pin_safe(pin)) {
    return 0;
  }
  return 1;
}

int setDigitalOutput(int pin, int value) {
  if (!is_pin_safe(pin)) {
    return 0;
  }
  return 1;
}

int readAnalogInput(int pin) {
  if (!is_pin_safe_ai(pin)) {
    return -1;
  }
  // Return pin * 100 as a mock value
  return pin * 100;
}

int setAnalogOutput(int pin, int value) {
  if (!is_pin_safe_ao(pin)) {
    return 0;
  }
  return 1;
}

// Network function implementations
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

// NeoPixel function implementations
bool hexToRgb(const char* hexColor, uint8_t& r, uint8_t& g, uint8_t& b) {
  // Check if the hex color is valid
  if (hexColor == NULL || hexColor[0] != '#' || strlen(hexColor) != 7) {
    return false;
  }

  // Check if all characters are valid hex digits
  for (int i = 1; i < 7; i++) {
    if (!isxdigit(hexColor[i])) {
      return false;
    }
  }

  // Convert hex to RGB
  char hex[3];
  
  // Red
  hex[0] = hexColor[1];
  hex[1] = hexColor[2];
  hex[2] = '\0';
  r = strtol(hex, NULL, 16);
  
  // Green
  hex[0] = hexColor[3];
  hex[1] = hexColor[4];
  hex[2] = '\0';
  g = strtol(hex, NULL, 16);
  
  // Blue
  hex[0] = hexColor[5];
  hex[1] = hexColor[6];
  hex[2] = '\0';
  b = strtol(hex, NULL, 16);
  
  return true;
}

// Stub for startCameraServer
void startCameraServer() {
  // This is just a stub for testing
}

#endif // TEST_MOCK_IMPLEMENTATIONS_H
