#include <AUnit.h>

// Include the test runner header which contains all test includes
#include "test_runner.h"

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  while (!Serial); // Wait for Serial to be ready
  
  // Print test banner
  Serial.println(F("ESP32-S3-ETH Web Camera - Test Runner"));
  Serial.println(F("======================================"));
  
  // Brief delay to allow serial connection to stabilize
  delay(1000);
}

void loop() {
  // Run all tests
  aunit::TestRunner::run();
}
