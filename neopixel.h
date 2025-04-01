#ifndef NEOPIXEL_H
#define NEOPIXEL_H

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>

// NeoPixel configuration
#define NEOPIXEL_PIN 38
#define NEOPIXEL_COUNT 1

// Global NeoPixel object
extern Adafruit_NeoPixel pixels;

// Initialize NeoPixel
void initNeoPixel();

// Set NeoPixel color (RGB)
void setNeoPixelColor(uint8_t r, uint8_t g, uint8_t b);

// Turn off NeoPixel
void turnOffNeoPixel();

// Convert hex color to RGB
bool hexToRgb(const char* hexColor, uint8_t& r, uint8_t& g, uint8_t& b);

#endif // NEOPIXEL_H
