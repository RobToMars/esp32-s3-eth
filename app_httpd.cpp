#include "esp_timer.h"
#include "esp_http_server.h"
#include "esp32-hal-ledc.h"
#include "sdkconfig.h"
#include <Arduino.h>
#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#endif
#include "utilities.h"
#include "network_config.h"
#include "neopixel.h"

// Array of safe digital output pins
const int safe_do_pins[] = {4, 5, 6, 7, 15, 16, 17, 18, 19, 20, 21, 35, 36, 37, 38, 39, 40, 41, 42, 45, 46};
const int num_safe_do_pins = sizeof(safe_do_pins) / sizeof(safe_do_pins[0]);

// Array of analog input pins
const int analog_input_pins[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
const int num_analog_input_pins = sizeof(analog_input_pins) / sizeof(analog_input_pins[0]);

// Array of analog output pins (same as digital output pins)
const int analog_output_pins[] = {4, 5, 6, 7, 15, 16, 17, 18, 19, 20, 21, 35, 36, 37, 38, 39, 40, 41, 42, 45, 46};
const int num_analog_output_pins = sizeof(analog_output_pins) / sizeof(analog_output_pins[0]);

// Track which pins have been initialized
bool do_pins_initialized[50] = {false};
bool ao_pins_initialized[50] = {false};
int ao_pin_channels[50] = {-1};
int next_pwm_channel = 0;

httpd_handle_t web_server = NULL;

// Check if a pin is safe to use
bool is_pin_safe(int pin) {
    for (int i = 0; i < num_safe_do_pins; i++) {
        if (pin == safe_do_pins[i]) {
            return true;
        }
    }
    return false;
}

// Check if a pin is safe for analog input
bool is_pin_safe_ai(int pin) {
    for (int i = 0; i < num_analog_input_pins; i++) {
        if (pin == analog_input_pins[i]) {
            return true;
        }
    }
    return false;
}

// Check if a pin is safe for analog output
bool is_pin_safe_ao(int pin) {
    for (int i = 0; i < num_analog_output_pins; i++) {
        if (pin == analog_output_pins[i]) {
            return true;
        }
    }
    return false;
}

// Initialize a digital output pin
int initDigitalOutput(int pin) {
    if (!is_pin_safe(pin)) {
        return 0;
    }
    
    if (!do_pins_initialized[pin]) {
        pinMode(pin, OUTPUT);
        do_pins_initialized[pin] = true;
    }
    
    return 1;
}

// Set a digital output pin
int setDigitalOutput(int pin, int value) {
    if (!is_pin_safe(pin)) {
        return 0;
    }
    
    if (!do_pins_initialized[pin]) {
        if (initDigitalOutput(pin) == 0) {
            return 0;
        }
    }
    
    digitalWrite(pin, value);
    return 1;
}

// Read from an analog input pin
int readAnalogInput(int pin) {
    if (!is_pin_safe_ai(pin)) {
        return -1;
    }
    
    return analogRead(pin);
}

// Set an analog output (PWM) pin
int setAnalogOutput(int pin, int value) {
    if (!is_pin_safe_ao(pin)) {
        return 0;
    }
    
    if (!ao_pins_initialized[pin]) {
        if (next_pwm_channel >= 16) {
            // All PWM channels are in use
            return 0;
        }
        
        // Configure PWM for this pin using Arduino's analogWrite
        pinMode(pin, OUTPUT);
        ao_pin_channels[pin] = next_pwm_channel;
        ao_pins_initialized[pin] = true;
        next_pwm_channel++;
    }
    
    // Write the value to the pin using analogWrite
    analogWrite(pin, value);
    return 1;
}

// Handler for index page
static esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "identity");
    
    // Simple HTML page
    const char* html = "<!DOCTYPE html>"
                       "<html>"
                       "<head>"
                       "<meta charset=\"utf-8\">"
                       "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                       "<title>ESP32-S3-ETH Control</title>"
                       "<style>"
                       "body{font-family:Arial,Helvetica,sans-serif;background:#f2f2f2;margin:0;padding:20px;color:#333;text-align:center}"
                       "h1{color:#0066cc;margin-bottom:20px}"
                       ".container{max-width:800px;margin:0 auto;background:white;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}"
                       ".section{margin-bottom:30px;padding:15px;background:#f9f9f9;border-radius:5px}"
                       "h2{color:#0099cc;margin-top:0}"
                       "button{background:#0099cc;color:white;border:none;padding:10px 15px;border-radius:5px;cursor:pointer;margin:5px}"
                       "button:hover{background:#0077aa}"
                       "input{padding:8px;margin:5px;border:1px solid #ddd;border-radius:4px}"
                       ".status{margin-top:10px;font-style:italic;color:#666}"
                       "</style>"
                       "</head>"
                       "<body>"
                       "<div class=\"container\">"
                       "<h1>ESP32-S3-ETH Control Panel</h1>"
                       
                       "<div class=\"section\">"
                       "<h2>GPIO Control</h2>"
                       "<div>"
                       "<label for=\"pin\">Pin:</label>"
                       "<input type=\"number\" id=\"pin\" min=\"0\" max=\"48\">"
                       "<button onclick=\"setHigh()\">Set HIGH</button>"
                       "<button onclick=\"setLow()\">Set LOW</button>"
                       "</div>"
                       "<div class=\"status\" id=\"gpio-status\"></div>"
                       "</div>"
                       
                       "<div class=\"section\">"
                       "<h2>Analog Input</h2>"
                       "<div>"
                       "<label for=\"ai-pin\">Pin:</label>"
                       "<input type=\"number\" id=\"ai-pin\" min=\"1\" max=\"10\">"
                       "<button onclick=\"readAnalog()\">Read Value</button>"
                       "</div>"
                       "<div class=\"status\" id=\"ai-status\"></div>"
                       "</div>"
                       
                       "<div class=\"section\">"
                       "<h2>Analog Output (PWM)</h2>"
                       "<div>"
                       "<label for=\"ao-pin\">Pin:</label>"
                       "<input type=\"number\" id=\"ao-pin\" min=\"0\" max=\"48\">"
                       "<label for=\"ao-value\">Value (0-255):</label>"
                       "<input type=\"number\" id=\"ao-value\" min=\"0\" max=\"255\" value=\"128\">"
                       "<button onclick=\"setPWM()\">Set PWM</button>"
                       "</div>"
                       "<div class=\"status\" id=\"ao-status\"></div>"
                       "</div>"
                       
                       "<div class=\"section\">"
                       "<h2>NeoPixel Control</h2>"
                       "<div>"
                       "<label for=\"red\">Red (0-255):</label>"
                       "<input type=\"number\" id=\"red\" min=\"0\" max=\"255\" value=\"0\">"
                       "<label for=\"green\">Green (0-255):</label>"
                       "<input type=\"number\" id=\"green\" min=\"0\" max=\"255\" value=\"0\">"
                       "<label for=\"blue\">Blue (0-255):</label>"
                       "<input type=\"number\" id=\"blue\" min=\"0\" max=\"255\" value=\"0\">"
                       "<button onclick=\"setNeoPixel()\">Set Color</button>"
                       "<button onclick=\"turnOffNeoPixel()\">Turn Off</button>"
                       "</div>"
                       "<div class=\"status\" id=\"neopixel-status\"></div>"
                       "</div>"
                       
                       "<div class=\"section\">"
                       "<h2>Network Configuration</h2>"
                       "<div id=\"network-info\">Loading...</div>"
                       "<button onclick=\"getNetworkConfig()\">Refresh</button>"
                       "</div>"
                       
                       "</div>"
                       
                       "<script>"
                       "function setHigh() {"
                       "  const pin = document.getElementById('pin').value;"
                       "  fetch(`/gpio/do?pin=${pin}&state=high`)"
                       "    .then(response => response.text())"
                       "    .then(data => {"
                       "      document.getElementById('gpio-status').innerText = data;"
                       "    });"
                       "}"
                       
                       "function setLow() {"
                       "  const pin = document.getElementById('pin').value;"
                       "  fetch(`/gpio/do?pin=${pin}&state=low`)"
                       "    .then(response => response.text())"
                       "    .then(data => {"
                       "      document.getElementById('gpio-status').innerText = data;"
                       "    });"
                       "}"
                       
                       "function readAnalog() {"
                       "  const pin = document.getElementById('ai-pin').value;"
                       "  fetch(`/gpio/ai/read?pin=${pin}`)"
                       "    .then(response => response.text())"
                       "    .then(data => {"
                       "      document.getElementById('ai-status').innerText = data;"
                       "    });"
                       "}"
                       
                       "function setPWM() {"
                       "  const pin = document.getElementById('ao-pin').value;"
                       "  const value = document.getElementById('ao-value').value;"
                       "  fetch(`/gpio/ao/set?pin=${pin}&value=${value}`)"
                       "    .then(response => response.text())"
                       "    .then(data => {"
                       "      document.getElementById('ao-status').innerText = data;"
                       "    });"
                       "}"
                       
                       "function setNeoPixel() {"
                       "  const r = document.getElementById('red').value;"
                       "  const g = document.getElementById('green').value;"
                       "  const b = document.getElementById('blue').value;"
                       "  fetch(`/neopixel/set?r=${r}&g=${g}&b=${b}`)"
                       "    .then(response => response.text())"
                       "    .then(data => {"
                       "      document.getElementById('neopixel-status').innerText = data;"
                       "    });"
                       "}"
                       
                       "function turnOffNeoPixel() {"
                       "  fetch('/neopixel/off')"
                       "    .then(response => response.text())"
                       "    .then(data => {"
                       "      document.getElementById('neopixel-status').innerText = data;"
                       "    });"
                       "}"
                       
                       "function getNetworkConfig() {"
                       "  fetch('/network/config/get')"
                       "    .then(response => response.json())"
                       "    .then(data => {"
                       "      let html = `<p>DHCP: ${data.dhcp_enabled ? 'Enabled' : 'Disabled'}</p>`;"
                       "      html += `<p>IP: ${data.ip.join('.')}</p>`;"
                       "      html += `<p>Gateway: ${data.gateway.join('.')}</p>`;"
                       "      html += `<p>Subnet: ${data.subnet.join('.')}</p>`;"
                       "      html += `<p>DNS1: ${data.dns1.join('.')}</p>`;"
                       "      html += `<p>DNS2: ${data.dns2.join('.')}</p>`;"
                       "      html += `<p>Hostname: ${data.hostname}</p>`;"
                       "      document.getElementById('network-info').innerHTML = html;"
                       "    });"
                       "}"
                       
                       "// Load network config on page load"
                       "getNetworkConfig();"
                       "</script>"
                       "</body>"
                       "</html>";
    
    return httpd_resp_send(req, html, strlen(html));
}

// Handler for digital output control
static esp_err_t gpio_do_handler(httpd_req_t *req) {
    char *buf;
    size_t buf_len;
    char pin_str[8] = {0};
    char state_str[8] = {0};
    
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = (char *)malloc(buf_len);
        if (!buf) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            if (httpd_query_key_value(buf, "pin", pin_str, sizeof(pin_str)) == ESP_OK &&
                httpd_query_key_value(buf, "state", state_str, sizeof(state_str)) == ESP_OK) {
                
                int pin = atoi(pin_str);
                int state = (strcmp(state_str, "high") == 0) ? HIGH : LOW;
                
                int result = setDigitalOutput(pin, state);
                
                char resp[64];
                if (result) {
                    sprintf(resp, "Pin %d set to %s", pin, state_str);
                } else {
                    sprintf(resp, "Error: Pin %d is not valid for digital output", pin);
                }
                
                httpd_resp_set_type(req, "text/plain");
                httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
                return httpd_resp_send(req, resp, strlen(resp));
            }
        }
        
        free(buf);
    }
    
    httpd_resp_send_404(req);
    return ESP_FAIL;
}

// Handler for multiple digital outputs
static esp_err_t gpio_do_all_handler(httpd_req_t *req) {
    char *buf;
    size_t buf_len;
    char pins_str[64] = {0};
    char states_str[64] = {0};
    
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = (char *)malloc(buf_len);
        if (!buf) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            if (httpd_query_key_value(buf, "pins", pins_str, sizeof(pins_str)) == ESP_OK &&
                httpd_query_key_value(buf, "states", states_str, sizeof(states_str)) == ESP_OK) {
                
                // Make copies for strtok
                char pins_copy[64];
                char states_copy[64];
                strcpy(pins_copy, pins_str);
                strcpy(states_copy, states_str);
                
                // Parse pins and states
                char *pin_token = strtok(pins_copy, ",");
                char *state_token = strtok(states_copy, ",");
                
                int success_count = 0;
                int fail_count = 0;
                
                while (pin_token != NULL && state_token != NULL) {
                    int pin = atoi(pin_token);
                    int state = (strcmp(state_token, "high") == 0) ? HIGH : LOW;
                    
                    int result = setDigitalOutput(pin, state);
                    if (result) {
                        success_count++;
                    } else {
                        fail_count++;
                    }
                    
                    pin_token = strtok(NULL, ",");
                    state_token = strtok(NULL, ",");
                }
                
                char resp[64];
                sprintf(resp, "Set %d pins successfully, %d failed", success_count, fail_count);
                
                httpd_resp_set_type(req, "text/plain");
                httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
                return httpd_resp_send(req, resp, strlen(resp));
            }
        }
        
        free(buf);
    }
    
    httpd_resp_send_404(req);
    return ESP_FAIL;
}

// Handler for analog input reading
static esp_err_t gpio_ai_read_handler(httpd_req_t *req) {
    char *buf;
    size_t buf_len;
    char pin_str[8] = {0};
    
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = (char *)malloc(buf_len);
        if (!buf) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            if (httpd_query_key_value(buf, "pin", pin_str, sizeof(pin_str)) == ESP_OK) {
                
                int pin = atoi(pin_str);
                int value = readAnalogInput(pin);
                
                char resp[64];
                if (value >= 0) {
                    sprintf(resp, "Pin %d analog value: %d", pin, value);
                } else {
                    sprintf(resp, "Error: Pin %d is not valid for analog input", pin);
                }
                
                httpd_resp_set_type(req, "text/plain");
                httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
                return httpd_resp_send(req, resp, strlen(resp));
            }
        }
        
        free(buf);
    }
    
    httpd_resp_send_404(req);
    return ESP_FAIL;
}

// Handler for analog output (PWM) setting
static esp_err_t gpio_ao_set_handler(httpd_req_t *req) {
    char *buf;
    size_t buf_len;
    char pin_str[8] = {0};
    char value_str[8] = {0};
    
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = (char *)malloc(buf_len);
        if (!buf) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            if (httpd_query_key_value(buf, "pin", pin_str, sizeof(pin_str)) == ESP_OK &&
                httpd_query_key_value(buf, "value", value_str, sizeof(value_str)) == ESP_OK) {
                
                int pin = atoi(pin_str);
                int value = atoi(value_str);
                
                // Ensure value is in range 0-255
                if (value < 0) value = 0;
                if (value > 255) value = 255;
                
                int result = setAnalogOutput(pin, value);
                
                char resp[64];
                if (result) {
                    sprintf(resp, "Pin %d PWM value set to %d", pin, value);
                } else {
                    sprintf(resp, "Error: Pin %d is not valid for analog output", pin);
                }
                
                httpd_resp_set_type(req, "text/plain");
                httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
                return httpd_resp_send(req, resp, strlen(resp));
            }
        }
        
        free(buf);
    }
    
    httpd_resp_send_404(req);
    return ESP_FAIL;
}

// Handler for GPIO overview
static esp_err_t gpio_overview_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    
    // Create a JSON response with GPIO information
    char *resp = (char *)malloc(4096);
    if (!resp) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    int len = 0;
    len += sprintf(resp + len, "{\n");
    len += sprintf(resp + len, "  \"safe_digital_pins\": [");
    
    for (int i = 0; i < num_safe_do_pins; i++) {
        len += sprintf(resp + len, "%d", safe_do_pins[i]);
        if (i < num_safe_do_pins - 1) {
            len += sprintf(resp + len, ", ");
        }
    }
    
    len += sprintf(resp + len, "],\n");
    len += sprintf(resp + len, "  \"analog_input_pins\": [");
    
    for (int i = 0; i < num_analog_input_pins; i++) {
        len += sprintf(resp + len, "%d", analog_input_pins[i]);
        if (i < num_analog_input_pins - 1) {
            len += sprintf(resp + len, ", ");
        }
    }
    
    len += sprintf(resp + len, "],\n");
    len += sprintf(resp + len, "  \"analog_output_pins\": [");
    
    for (int i = 0; i < num_analog_output_pins; i++) {
        len += sprintf(resp + len, "%d", analog_output_pins[i]);
        if (i < num_analog_output_pins - 1) {
            len += sprintf(resp + len, ", ");
        }
    }
    
    len += sprintf(resp + len, "],\n");
    len += sprintf(resp + len, "  \"initialized_pins\": [");
    
    bool first = true;
    for (int i = 0; i < 50; i++) {
        if (do_pins_initialized[i] || ao_pins_initialized[i]) {
            if (!first) {
                len += sprintf(resp + len, ", ");
            }
            len += sprintf(resp + len, "%d", i);
            first = false;
        }
    }
    
    len += sprintf(resp + len, "]\n");
    len += sprintf(resp + len, "}\n");
    
    httpd_resp_send(req, resp, len);
    free(resp);
    return ESP_OK;
}

// Handler for NeoPixel color setting
static esp_err_t neopixel_set_handler(httpd_req_t *req) {
    char *buf;
    size_t buf_len;
    char r_str[8] = {0};
    char g_str[8] = {0};
    char b_str[8] = {0};
    
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = (char *)malloc(buf_len);
        if (!buf) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            if (httpd_query_key_value(buf, "r", r_str, sizeof(r_str)) == ESP_OK &&
                httpd_query_key_value(buf, "g", g_str, sizeof(g_str)) == ESP_OK &&
                httpd_query_key_value(buf, "b", b_str, sizeof(b_str)) == ESP_OK) {
                
                int r = atoi(r_str);
                int g = atoi(g_str);
                int b = atoi(b_str);
                
                // Ensure values are in range 0-255
                if (r < 0) r = 0;
                if (r > 255) r = 255;
                if (g < 0) g = 0;
                if (g > 255) g = 255;
                if (b < 0) b = 0;
                if (b > 255) b = 255;
                
                setNeoPixelColor(r, g, b);
                
                char resp[64];
                sprintf(resp, "NeoPixel color set to RGB(%d, %d, %d)", r, g, b);
                
                httpd_resp_set_type(req, "text/plain");
                httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
                return httpd_resp_send(req, resp, strlen(resp));
            }
        }
        
        free(buf);
    }
    
    httpd_resp_send_404(req);
    return ESP_FAIL;
}

// Handler for turning off NeoPixel
static esp_err_t neopixel_off_handler(httpd_req_t *req) {
    turnOffNeoPixel();
    
    const char *resp = "NeoPixel turned off";
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, resp, strlen(resp));
}

// Handler for getting network configuration
static esp_err_t network_config_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    
    char resp[512];
    int len = 0;
    
    len += sprintf(resp + len, "{\n");
    len += sprintf(resp + len, "  \"dhcp_enabled\": %s,\n", networkConfig.dhcp_enabled ? "true" : "false");
    
    len += sprintf(resp + len, "  \"ip\": [%d, %d, %d, %d],\n", 
                  networkConfig.ip[0], networkConfig.ip[1], networkConfig.ip[2], networkConfig.ip[3]);
    
    len += sprintf(resp + len, "  \"gateway\": [%d, %d, %d, %d],\n", 
                  networkConfig.gateway[0], networkConfig.gateway[1], networkConfig.gateway[2], networkConfig.gateway[3]);
    
    len += sprintf(resp + len, "  \"subnet\": [%d, %d, %d, %d],\n", 
                  networkConfig.subnet[0], networkConfig.subnet[1], networkConfig.subnet[2], networkConfig.subnet[3]);
    
    len += sprintf(resp + len, "  \"dns1\": [%d, %d, %d, %d],\n", 
                  networkConfig.dns1[0], networkConfig.dns1[1], networkConfig.dns1[2], networkConfig.dns1[3]);
    
    len += sprintf(resp + len, "  \"dns2\": [%d, %d, %d, %d],\n", 
                  networkConfig.dns2[0], networkConfig.dns2[1], networkConfig.dns2[2], networkConfig.dns2[3]);
    
    len += sprintf(resp + len, "  \"hostname\": \"%s\"\n", networkConfig.hostname);
    len += sprintf(resp + len, "}\n");
    
    return httpd_resp_send(req, resp, len);
}

// Handler for setting network configuration
static esp_err_t network_config_set_handler(httpd_req_t *req) {
    char *buf;
    size_t buf_len;
    char dhcp_str[8] = {0};
    char ip_str[32] = {0};
    char gateway_str[32] = {0};
    char subnet_str[32] = {0};
    char dns1_str[32] = {0};
    char dns2_str[32] = {0};
    char hostname_str[32] = {0};
    char apply_str[8] = {0};
    
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = (char *)malloc(buf_len);
        if (!buf) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            httpd_query_key_value(buf, "dhcp", dhcp_str, sizeof(dhcp_str));
            httpd_query_key_value(buf, "ip", ip_str, sizeof(ip_str));
            httpd_query_key_value(buf, "gateway", gateway_str, sizeof(gateway_str));
            httpd_query_key_value(buf, "subnet", subnet_str, sizeof(subnet_str));
            httpd_query_key_value(buf, "dns1", dns1_str, sizeof(dns1_str));
            httpd_query_key_value(buf, "dns2", dns2_str, sizeof(dns2_str));
            httpd_query_key_value(buf, "hostname", hostname_str, sizeof(hostname_str));
            httpd_query_key_value(buf, "apply", apply_str, sizeof(apply_str));
            
            // Update network configuration
            if (strlen(dhcp_str) > 0) {
                networkConfig.dhcp_enabled = (strcmp(dhcp_str, "true") == 0);
            }
            
            if (strlen(ip_str) > 0) {
                // Parse IP address (format: 192.168.1.100)
                sscanf(ip_str, "%d.%d.%d.%d", 
                       &networkConfig.ip[0], &networkConfig.ip[1], 
                       &networkConfig.ip[2], &networkConfig.ip[3]);
            }
            
            if (strlen(gateway_str) > 0) {
                sscanf(gateway_str, "%d.%d.%d.%d", 
                       &networkConfig.gateway[0], &networkConfig.gateway[1], 
                       &networkConfig.gateway[2], &networkConfig.gateway[3]);
            }
            
            if (strlen(subnet_str) > 0) {
                sscanf(subnet_str, "%d.%d.%d.%d", 
                       &networkConfig.subnet[0], &networkConfig.subnet[1], 
                       &networkConfig.subnet[2], &networkConfig.subnet[3]);
            }
            
            if (strlen(dns1_str) > 0) {
                sscanf(dns1_str, "%d.%d.%d.%d", 
                       &networkConfig.dns1[0], &networkConfig.dns1[1], 
                       &networkConfig.dns1[2], &networkConfig.dns1[3]);
            }
            
            if (strlen(dns2_str) > 0) {
                sscanf(dns2_str, "%d.%d.%d.%d", 
                       &networkConfig.dns2[0], &networkConfig.dns2[1], 
                       &networkConfig.dns2[2], &networkConfig.dns2[3]);
            }
            
            if (strlen(hostname_str) > 0) {
                strncpy(networkConfig.hostname, hostname_str, sizeof(networkConfig.hostname) - 1);
            }
            
            // Save configuration
            saveNetworkConfig();
            
            // Apply configuration if requested
            bool apply = (strcmp(apply_str, "true") == 0);
            
            char resp[128];
            if (apply) {
                sprintf(resp, "Network configuration updated and will be applied on next restart");
            } else {
                sprintf(resp, "Network configuration updated");
            }
            
            httpd_resp_set_type(req, "text/plain");
            httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
            httpd_resp_send(req, resp, strlen(resp));
            
            free(buf);
            
            // Restart if apply was requested
            if (apply) {
                // Wait a moment for the response to be sent
                delay(1000);
                ESP.restart();
            }
            
            return ESP_OK;
        }
        
        free(buf);
    }
    
    httpd_resp_send_404(req);
    return ESP_FAIL;
}

// Handler for restarting the device
static esp_err_t restart_handler(httpd_req_t *req) {
    const char *resp = "Restarting device...";
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, resp, strlen(resp));
    
    // Wait a moment for the response to be sent
    delay(1000);
    ESP.restart();
    
    return ESP_OK;
}

void startWebServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;
    
    if (httpd_start(&web_server, &config) == ESP_OK) {
        // Index page
        httpd_uri_t index_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = index_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(web_server, &index_uri);
        
        // GPIO handlers
        httpd_uri_t gpio_do_uri = {
            .uri = "/gpio/do",
            .method = HTTP_GET,
            .handler = gpio_do_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(web_server, &gpio_do_uri);
        
        httpd_uri_t gpio_do_all_uri = {
            .uri = "/gpio/do/all",
            .method = HTTP_GET,
            .handler = gpio_do_all_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(web_server, &gpio_do_all_uri);
        
        httpd_uri_t gpio_ai_read_uri = {
            .uri = "/gpio/ai/read",
            .method = HTTP_GET,
            .handler = gpio_ai_read_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(web_server, &gpio_ai_read_uri);
        
        httpd_uri_t gpio_ao_set_uri = {
            .uri = "/gpio/ao/set",
            .method = HTTP_GET,
            .handler = gpio_ao_set_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(web_server, &gpio_ao_set_uri);
        
        httpd_uri_t gpio_overview_uri = {
            .uri = "/gpio/overview",
            .method = HTTP_GET,
            .handler = gpio_overview_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(web_server, &gpio_overview_uri);
        
        // NeoPixel handlers
        httpd_uri_t neopixel_set_uri = {
            .uri = "/neopixel/set",
            .method = HTTP_GET,
            .handler = neopixel_set_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(web_server, &neopixel_set_uri);
        
        httpd_uri_t neopixel_off_uri = {
            .uri = "/neopixel/off",
            .method = HTTP_GET,
            .handler = neopixel_off_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(web_server, &neopixel_off_uri);
        
        // Network configuration handlers
        httpd_uri_t network_config_get_uri = {
            .uri = "/network/config/get",
            .method = HTTP_GET,
            .handler = network_config_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(web_server, &network_config_get_uri);
        
        httpd_uri_t network_config_set_uri = {
            .uri = "/network/config/set",
            .method = HTTP_GET,
            .handler = network_config_set_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(web_server, &network_config_set_uri);
        
        // Restart handler
        httpd_uri_t restart_uri = {
            .uri = "/restart",
            .method = HTTP_GET,
            .handler = restart_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(web_server, &restart_uri);
        
        Serial.println("Web server started");
    } else {
        Serial.println("Failed to start web server");
    }
}
