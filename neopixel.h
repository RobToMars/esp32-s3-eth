#ifndef NEOPIXEL_H
#define NEOPIXEL_H

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>

// NeoPixel configuration
#define NEOPIXEL_PIN 38
#define NEOPIXEL_COUNT 1

// Global NeoPixel object
Adafruit_NeoPixel pixels(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// Initialize NeoPixel
void initNeoPixel() {
    pixels.begin();
    pixels.clear();
    pixels.show();
    Serial.println("NeoPixel initialized");
}

// Set NeoPixel color (RGB)
void setNeoPixelColor(uint8_t r, uint8_t g, uint8_t b) {
    pixels.setPixelColor(0, pixels.Color(r, g, b));
    pixels.show();
}

// Turn off NeoPixel
void turnOffNeoPixel() {
    pixels.clear();
    pixels.show();
}

// Convert hex color to RGB
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

#endif // NEOPIXEL_H
