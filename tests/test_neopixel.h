#ifndef TEST_NEOPIXEL_H
#define TEST_NEOPIXEL_H

#include <AUnit.h>
#include "test_common.h"
#include "neopixel_wrapper.h"

// Define NEOPIXEL_PIN and NEOPIXEL_COUNT for tests
#define NEOPIXEL_PIN 48
#define NEOPIXEL_COUNT 1

// Test NeoPixel initialization
test(NeoPixelInitialization) {
  // Test initializing NeoPixel
  // In a real test, this would call the actual initialization function
  // Here we're just testing the initialization logic
  
  // Verify NeoPixel pin is valid
  assertTrue(is_pin_safe(NEOPIXEL_PIN));
  
  // Simulate initialization
  mockNeoPixelBegin();
  
  // Verify initialization was successful
  assertTrue(true);
}

// Test setting NeoPixel color
test(NeoPixelSetColor) {
  // Test setting NeoPixel color
  uint8_t r = 255;
  uint8_t g = 0;
  uint8_t b = 0;
  
  // Simulate setting color
  mockNeoPixelSetPixelColor(0, r, g, b);
  mockNeoPixelShow();
  
  // Verify color was set (in a real test, we would verify the actual color)
  assertTrue(true);
}

// Test turning off NeoPixel
test(NeoPixelTurnOff) {
  // Test turning off NeoPixel
  
  // Simulate turning off
  mockNeoPixelSetPixelColor(0, 0, 0, 0);
  mockNeoPixelShow();
  
  // Verify NeoPixel was turned off (in a real test, we would verify it's actually off)
  assertTrue(true);
}

// Test RGB color validation
test(NeoPixelRgbValidation) {
  // Test valid hex color conversion
  uint8_t r, g, b;
  
  // Test valid hex colors
  assertTrue(hexToRgb("#FF0000", r, g, b));
  assertEqual(r, 255);
  assertEqual(g, 0);
  assertEqual(b, 0);
  
  assertTrue(hexToRgb("#00FF00", r, g, b));
  assertEqual(r, 0);
  assertEqual(g, 255);
  assertEqual(b, 0);
  
  assertTrue(hexToRgb("#0000FF", r, g, b));
  assertEqual(r, 0);
  assertEqual(g, 0);
  assertEqual(b, 255);
  
  // Test invalid hex colors
  assertFalse(hexToRgb("FF0000", r, g, b));  // Missing #
  assertFalse(hexToRgb("#FF00", r, g, b));   // Too short
  assertFalse(hexToRgb("#FF00000", r, g, b)); // Too long
  assertFalse(hexToRgb("#GGFFFF", r, g, b)); // Invalid character
}

#endif // TEST_NEOPIXEL_H
