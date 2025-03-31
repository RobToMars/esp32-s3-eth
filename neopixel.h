#include <Adafruit_NeoPixel.h>
#include "esp_http_server.h"

// NeoPixel configuration
#define NEOPIXEL_PIN 21  // Pin for onboard NeoPixel as per example code
#define NEOPIXEL_COUNT 1 // Default count for onboard NeoPixel, can be changed

// NeoPixel object
Adafruit_NeoPixel pixels(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// Initialize NeoPixel
void initNeoPixel() {
  pixels.begin();
  pixels.clear();
  pixels.show();
}

// Convert hex color string to RGB values
bool hexToRgb(const char* hexColor, uint8_t& r, uint8_t& g, uint8_t& b) {
  // Skip # if present
  if (hexColor[0] == '#') {
    hexColor++;
  }
  
  // Check length
  int len = strlen(hexColor);
  if (len != 6) {
    return false;
  }
  
  // Convert hex to RGB
  char hex[3] = {0};
  
  // Red
  hex[0] = hexColor[0];
  hex[1] = hexColor[1];
  r = strtol(hex, NULL, 16);
  
  // Green
  hex[0] = hexColor[2];
  hex[1] = hexColor[3];
  g = strtol(hex, NULL, 16);
  
  // Blue
  hex[0] = hexColor[4];
  hex[1] = hexColor[5];
  b = strtol(hex, NULL, 16);
  
  return true;
}

// Handler for setting NeoPixel color
static esp_err_t neopixel_set_handler(httpd_req_t *req) {
  char query[256];
  char color_str[32] = {0};
  char brightness_str[32] = {0};
  
  // Get query parameters
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }
  
  // Extract color
  if (httpd_query_key_value(query, "color", color_str, sizeof(color_str)) != ESP_OK) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    char response[128];
    sprintf(response, "{\"error\":\"Missing color parameter\",\"success\":false}");
    return httpd_resp_send(req, response, strlen(response));
  }
  
  // Extract brightness (optional)
  int brightness = 255; // Default to full brightness
  if (httpd_query_key_value(query, "brightness", brightness_str, sizeof(brightness_str)) == ESP_OK) {
    brightness = atoi(brightness_str);
    if (brightness < 0) brightness = 0;
    if (brightness > 255) brightness = 255;
  }
  
  // Set brightness
  pixels.setBrightness(brightness);
  
  // Convert color and set pixel
  uint8_t r, g, b;
  if (hexToRgb(color_str, r, g, b)) {
    pixels.setPixelColor(0, pixels.Color(r, g, b));
    pixels.show();
    
    // Send response
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    char response[128];
    sprintf(response, "{\"color\":\"#%02X%02X%02X\",\"brightness\":%d,\"success\":true}", r, g, b, brightness);
    return httpd_resp_send(req, response, strlen(response));
  } else {
    // Invalid color format
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    char response[128];
    sprintf(response, "{\"error\":\"Invalid color format. Use hexadecimal format (e.g., FF0000 for red)\",\"success\":false}");
    return httpd_resp_send(req, response, strlen(response));
  }
}

// Handler for turning off NeoPixel
static esp_err_t neopixel_off_handler(httpd_req_t *req) {
  // Turn off the NeoPixel
  pixels.clear();
  pixels.show();
  
  // Send response
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  char response[64];
  sprintf(response, "{\"status\":\"off\",\"success\":true}");
  return httpd_resp_send(req, response, strlen(response));
}
