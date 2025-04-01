#ifndef TEST_RUNNER_H
#define TEST_RUNNER_H

#include <AUnit.h>

// Include mock implementations first to provide function definitions
#include "../test_mock_implementations.h"

// Forward declarations of functions to avoid redefinition errors
void initNeoPixel();
bool hexToRgb(const char* hexColor, uint8_t& r, uint8_t& g, uint8_t& b);
bool isValidIP(const char* ip);
bool is_pin_safe(int pin);
bool is_pin_safe_ai(int pin);
bool is_pin_safe_ao(int pin);
int initDigitalOutput(int pin);
int setDigitalOutput(int pin, int value);
int readAnalogInput(int pin);
int setAnalogOutput(int pin, int value);
void startCameraServer();

// Include test files with correct relative paths
#include "../test_common.h"
#include "../test_gpio.h"
#include "../test_network.h"
#include "../test_neopixel.h"
#include "../test_integration.h"

#endif // TEST_RUNNER_H
