#ifndef NEOPIXEL_WRAPPER_H
#define NEOPIXEL_WRAPPER_H

// Include guards for the original files
#ifndef NEOPIXEL_INCLUDED
#define NEOPIXEL_INCLUDED

// Forward declarations for functions in neopixel.h
void initNeoPixel();
bool hexToRgb(const char* hexColor, uint8_t& r, uint8_t& g, uint8_t& b);

#endif // NEOPIXEL_INCLUDED

#endif // NEOPIXEL_WRAPPER_H
