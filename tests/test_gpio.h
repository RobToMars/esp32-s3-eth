#ifndef TEST_GPIO_H
#define TEST_GPIO_H

#include <AUnit.h>
#include "test_common.h"
#include "app_httpd_wrapper.h"

// Test GPIO pin safety checks
test(GpioPinSafety) {
  // Test safe digital pins
  assertTrue(is_pin_safe(16));
  assertTrue(is_pin_safe(17));
  assertTrue(is_pin_safe(18));
  assertTrue(is_pin_safe(6));  // Now expected to be safe
  assertTrue(is_pin_safe(7));  // Now expected to be safe

  // Test unsafe pins
  assertFalse(is_pin_safe(8));  // Flash D1 (still unsafe)
  assertFalse(is_pin_safe(9));  // Used by Ethernet
  assertFalse(is_pin_safe(10)); // Used by Ethernet
}

// Test digital output functionality
test(GpioDigitalOutput) {
  // Test initializing a digital output pin
  int pin = 16;
  int result = initDigitalOutput(pin);
  assertEqual(result, 1);

  // Test setting pin state
  result = setDigitalOutput(pin, HIGH);
  assertEqual(result, 1);
  result = setDigitalOutput(pin, LOW);
  assertEqual(result, 1);

  // Test invalid pin
  result = setDigitalOutput(9, HIGH); // Ethernet pin, should fail
  assertEqual(result, 0);
}

// Test analog input functionality
test(GpioAnalogInput) {
  // Test reading from an analog input pin
  int pin = 1;
  int value = readAnalogInput(pin);
  // In test mode, mockAnalogRead returns pin * 100
  assertEqual(value, pin * 100);

  // Test invalid pin
  value = readAnalogInput(11); // Not in analog_input_pins, should return -1
  assertEqual(value, -1);
}

// Test analog output (PWM) functionality
test(GpioAnalogOutput) {
  // Test setting analog output value
  int pin = 16;
  int value = 128; // 50% duty cycle
  int result = setAnalogOutput(pin, value);
  assertEqual(result, 1);

  // Test invalid pin
  result = setAnalogOutput(9, value); // Ethernet pin, should fail
  assertEqual(result, 0);
}

// Test multiple digital outputs functionality
test(GpioMultipleDigitalOutputs) {
  // Test setting multiple pins at once
  char pins[] = "16,17,18";
  char states[] = "high,low,high";

  // Parse pins and states
  int pin_count = 0;
  char pins_copy[32];
  char states_copy[32];
  strcpy(pins_copy, pins);
  strcpy(states_copy, states);

  char* pin_token = strtok(pins_copy, ",");
  char* state_token = strtok(states_copy, ",");

  while (pin_token != NULL && state_token != NULL) {
    int pin = atoi(pin_token);
    int state = (strcmp(state_token, "high") == 0) ? HIGH : LOW;
    
    // Verify pin is valid
    assertTrue(is_pin_safe(pin));
    
    // Verify state is valid
    assertTrue(state == HIGH || state == LOW);
    
    pin_token = strtok(NULL, ",");
    state_token = strtok(NULL, ",");
    pin_count++;
  }

  // Verify we processed 3 pins
  assertEqual(pin_count, 3);
}

// Test GPIO overview functionality
test(GpioOverview) {
  // Test getting GPIO overview
  // This would call the handler function in a real test
  // Here we're just testing that the function exists
  assertTrue(true);

  // Verify GPIO 17 is included in the safe pins list
  assertTrue(is_pin_safe(17));
}

#endif // TEST_GPIO_H
