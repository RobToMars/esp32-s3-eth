# ESP32-S3-ETH Web Camera Test Documentation

This document provides detailed information about the test suite for the ESP32-S3-ETH Web Camera project.

## Test Overview

The test suite is designed to verify the functionality of various components of the ESP32-S3-ETH Web Camera project, including:

1. GPIO functionality (digital and analog I/O)
2. Network configuration and management
3. NeoPixel (RGB LED) control
4. Integration between different components

## Test Framework

The tests use the AUnit testing framework, which is a unit testing framework for Arduino platforms. AUnit is inspired by ArduinoUnit and Google Test, and it allows tests to run directly on the ESP32 hardware.

## Test Structure

The tests are organized into the following files:

- `test_gpio.h`: Tests for GPIO functionality
- `test_network.h`: Tests for network configuration
- `test_neopixel.h`: Tests for NeoPixel control
- `test_integration.h`: Tests for integration between components
- `test_runner/test_runner.ino`: Main sketch that runs all tests

## Test Categories

### GPIO Tests

The GPIO tests verify:
- Pin safety checks (is_pin_safe, is_pin_safe_ai, is_pin_safe_ao)
- Digital output initialization and state setting
- Analog input reading
- Analog output (PWM) functionality
- Multiple digital outputs functionality
- GPIO overview functionality
- Confirmation that GPIO 17 is included in the safe pins list

### Network Tests

The network tests verify:
- Network configuration initialization
- Setting static IP configuration
- Setting DHCP configuration
- Network configuration get/set handlers
- Restart handler
- IP address validation

### NeoPixel Tests

The NeoPixel tests verify:
- NeoPixel initialization
- Setting NeoPixel color
- Turning off NeoPixel
- NeoPixel set/off handlers
- RGB color validation

### Integration Tests

The integration tests verify:
- Integration between GPIO and NeoPixel
- Integration between Network and GPIO
- Integration between Camera and Network
- HTTP server initialization and endpoint registration

## Running the Tests

To run the tests:

1. Install the AUnit library through the Arduino IDE Library Manager:
   - Open Arduino IDE
   - Navigate to Sketch > Include Library > Manage Libraries
   - Search for "AUnit"
   - Click "Install"

2. Open the test runner sketch in Arduino IDE:
   - Open `tests/test_runner/test_runner.ino`

3. Connect your ESP32-S3-ETH board to your computer

4. Select the correct board and port in Arduino IDE:
   - Board: "ESP32S3 Dev Module" (under ESP32 Arduino)
   - Port: The port your ESP32 is connected to

5. Upload the sketch to your ESP32-S3-ETH board

6. Open the Serial Monitor to view the test results:
   - Set the baud rate to 115200

## Interpreting Test Results

The test results will be displayed in the Serial Monitor. Each test will be reported as either:

- PASSED: The test completed successfully
- FAILED: The test failed one or more assertions
- SKIPPED: The test was skipped (if applicable)

At the end of the test run, a summary will be displayed showing the total number of tests, passes, failures, and skips.

## Test Mocking

Since many functions in the project interact with hardware, the tests use mock functions to simulate hardware behavior. This allows testing of logic without requiring actual hardware interaction.

For example:
- `mockDigitalRead` and `mockDigitalWrite` simulate GPIO operations
- `mockAnalogRead` simulates analog input
- `mockLedcAttach`, `mockLedcWrite`, and `mockLedcDetach` simulate PWM operations
- `mockSaveNetworkConfig` and `mockLoadNetworkConfig` simulate network configuration storage
- `mockNeoPixelBegin`, `mockNeoPixelSetPixelColor`, and `mockNeoPixelShow` simulate NeoPixel operations

## Adding New Tests

To add new tests:

1. Create a new test file or add to an existing one
2. Include the AUnit framework: `#include <AUnit.h>`
3. Define test cases using the `test()` macro
4. Include your test file in `test_runner.ino`

Example test case:

```cpp
test(MyNewTestCase) {
  // Test setup
  int result = myFunction();
  
  // Assertions
  assertEqual(result, expectedValue);
}
```

## Continuous Integration

These tests can be integrated into a continuous integration pipeline using PlatformIO, which supports automated testing for Arduino projects.

## Troubleshooting

If you encounter issues running the tests:

1. Ensure the AUnit library is installed correctly
2. Verify that your ESP32-S3-ETH board is properly connected
3. Check that you've selected the correct board and port in Arduino IDE
4. Ensure the Serial Monitor is set to the correct baud rate (115200)
5. If tests are failing, check the error messages for details on what's failing and why

## Limitations

- Some tests use mocks and may not fully test hardware interactions
- The tests assume a specific hardware configuration (ESP32-S3-ETH board with OV2640 camera)
- Network tests may behave differently depending on your network environment
