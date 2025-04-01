# ESP32-S3-ETH Web Camera Tests

This directory contains unit tests for the ESP32-S3-ETH Web Camera project. The tests are designed to verify the functionality of various components of the system.

## Test Structure

The tests are organized into the following categories:

1. **GPIO Tests**: Tests for digital and analog I/O functionality
2. **Network Tests**: Tests for network configuration and management
3. **NeoPixel Tests**: Tests for RGB LED control
4. **Integration Tests**: Tests that verify multiple components working together

## Test Framework

These tests use the AUnit testing framework, which is a unit testing framework for Arduino platforms. AUnit is inspired by ArduinoUnit and Google Test, and it allows tests to run directly on the ESP32 hardware.

## Running the Tests

To run the tests:

1. Install the AUnit library through the Arduino IDE Library Manager
   - Open Arduino IDE
   - Navigate to Sketch > Include Library > Manage Libraries
   - Search for "AUnit"
   - Click "Install"

2. Open the test runner sketch in Arduino IDE
   - Open `tests/test_runner/test_runner.ino`

3. Upload the sketch to your ESP32-S3-ETH board

4. Open the Serial Monitor to view the test results
   - Set the baud rate to 115200

## Test Files

- `test_gpio.h`: Tests for GPIO functionality
- `test_network.h`: Tests for network configuration
- `test_neopixel.h`: Tests for NeoPixel control
- `test_integration.h`: Integration tests
- `test_runner/test_runner.ino`: Main sketch that runs all tests

## Adding New Tests

To add new tests:

1. Create a new test file in the `tests` directory
2. Include the AUnit framework: `#include <AUnit.h>`
3. Define test cases using the `test()` macro
4. Include your test file in `test_runner.ino`

Example test case:

```cpp
#include <AUnit.h>

test(MyTestCase) {
  // Test setup
  int result = myFunction();
  
  // Assertions
  assertEqual(result, expectedValue);
}
```

## Test Mocking

For functions that depend on hardware, mock implementations are provided to simulate hardware behavior. These mocks allow testing of logic without requiring actual hardware interaction.

## Continuous Integration

These tests can be run as part of a continuous integration pipeline using PlatformIO, which supports automated testing for Arduino projects.
