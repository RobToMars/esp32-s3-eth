#include <esp_http_server.h>
#include <esp_timer.h>
#include <esp_camera.h>
#include <Arduino.h>
#include "camera_index.h"
#include "network_config.h"
#include "neopixel.h"

// Safe pins for digital output
const int safe_do_pins[] = {
  4, 5, 6, 7, 15, 16, 17, 18, 19, 20, 21, 35, 36, 37, 38, 39, 40, 41, 42, 45, 46
};

// Safe pins for digital input
const int safe_di_pins[] = {
  4, 5, 6, 7, 15, 16, 17, 18, 19, 20, 21, 35, 36, 37, 38, 39, 40, 41, 42, 45, 46
};

// Safe pins for analog output (PWM)
const int analog_output_pins[] = {
  4, 5, 6, 7, 15, 16, 17, 18, 19, 20, 21, 35, 36, 37, 38, 39, 40, 41, 42, 45, 46
};

// Safe pins for analog input (ADC)
const int analog_input_pins[] = {
  1, 2, 3, 4, 5, 6, 7, 8, 9, 10
};

// Pin initialization tracking
bool do_pins_initialized[50] = {false};
bool di_pins_initialized[50] = {false};
bool ao_pins_initialized[50] = {false};
bool ai_pins_initialized[50] = {false};

// Stream content type and boundary definitions
#define PART_BOUNDARY "123456789000000000000987654321"
#define _STREAM_CONTENT_TYPE "multipart/x-mixed-replace;boundary=" PART_BOUNDARY
#define _STREAM_BOUNDARY "\r\n--" PART_BOUNDARY "\r\n"
#define _STREAM_PART "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n"

// Check if a pin is safe to use
bool is_pin_safe(int pin) {
  for (int i = 0; i < sizeof(safe_do_pins) / sizeof(safe_do_pins[0]); i++) {
    if (safe_do_pins[i] == pin) {
      return true;
    }
  }
  return false;
}

// Check if a pin is safe for analog input
bool is_pin_safe_ai(int pin) {
  for (int i = 0; i < sizeof(analog_input_pins) / sizeof(analog_input_pins[0]); i++) {
    if (analog_input_pins[i] == pin) {
      return true;
    }
  }
  return false;
}

// Check if a pin is safe for analog output
bool is_pin_safe_ao(int pin) {
  for (int i = 0; i < sizeof(analog_output_pins) / sizeof(analog_output_pins[0]); i++) {
    if (analog_output_pins[i] == pin) {
      return true;
    }
  }
  return false;
}

// JPEG HTTP stream handler
typedef struct {
    httpd_req_t *req;
    size_t len;
} jpg_chunking_t;

static size_t jpg_encode_stream(void *arg, size_t index, const void *data, size_t len) {
    jpg_chunking_t *j = (jpg_chunking_t *)arg;
    if (!index) {
        j->len = 0;
    }
    if (httpd_resp_send_chunk(j->req, (const char *)data, len) != ESP_OK) {
        return 0;
    }
    j->len += len;
    return len;
}

static esp_err_t capture_handler(httpd_req_t *req){
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    int64_t fr_start = esp_timer_get_time();

    fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    size_t fb_len = 0;
    if(fb->format == PIXFORMAT_JPEG){
        fb_len = fb->len;
        res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    } else {
        jpg_chunking_t jchunk = {req, 0};
        res = frame2jpg_cb(fb, 80, jpg_encode_stream, &jchunk)?ESP_OK:ESP_FAIL;
        httpd_resp_send_chunk(req, NULL, 0);
        fb_len = jchunk.len;
    }
    esp_camera_fb_return(fb);
    int64_t fr_end = esp_timer_get_time();
    Serial.printf("JPG: %uB %ums\n", (uint32_t)(fb_len), (uint32_t)((fr_end - fr_start)/1000));
    return res;
}

static esp_err_t stream_handler(httpd_req_t *req){
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t * _jpg_buf = NULL;
    char * part_buf[64];
    
    static int64_t last_frame = 0;
    if(!last_frame) {
        last_frame = esp_timer_get_time();
    }

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if(res != ESP_OK){
        return res;
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    while(true){
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Camera capture failed");
            res = ESP_FAIL;
        } else {
            if(fb->format != PIXFORMAT_JPEG){
                bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
                esp_camera_fb_return(fb);
                fb = NULL;
                if(!jpeg_converted){
                    Serial.println("JPEG compression failed");
                    res = ESP_FAIL;
                }
            } else {
                _jpg_buf_len = fb->len;
                _jpg_buf = fb->buf;
            }
        }
        if(res == ESP_OK){
            size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
            res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }
        if(fb){
            esp_camera_fb_return(fb);
            fb = NULL;
            _jpg_buf = NULL;
        } else if(_jpg_buf){
            free(_jpg_buf);
            _jpg_buf = NULL;
        }
        if(res != ESP_OK){
            break;
        }
        int64_t fr_end = esp_timer_get_time();
        int64_t frame_time = fr_end - last_frame;
        last_frame = fr_end;
        frame_time /= 1000;
        Serial.printf("MJPG: %uB %ums (%.1ffps)\n",
            (uint32_t)(_jpg_buf_len),
            (uint32_t)frame_time, 1000.0 / (uint32_t)frame_time);
    }

    last_frame = 0;
    return res;
}

static esp_err_t cmd_handler(httpd_req_t *req){
    char*  buf;
    size_t buf_len;
    char variable[32] = {0,};
    char value[32] = {0,};

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = (char*)malloc(buf_len);
        if(!buf){
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            if (httpd_query_key_value(buf, "var", variable, sizeof(variable)) == ESP_OK &&
                httpd_query_key_value(buf, "val", value, sizeof(value)) == ESP_OK) {
            } else {
                free(buf);
                httpd_resp_send_404(req);
                return ESP_FAIL;
            }
        } else {
            free(buf);
            httpd_resp_send_404(req);
            return ESP_FAIL;
        }
        free(buf);
    } else {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    int val = atoi(value);
    sensor_t * s = esp_camera_sensor_get();
    int res = 0;

    if(!strcmp(variable, "framesize")) {
        if(s->pixformat == PIXFORMAT_JPEG) res = s->set_framesize(s, (framesize_t)val);
    }
    else if(!strcmp(variable, "quality")) res = s->set_quality(s, val);
    else if(!strcmp(variable, "contrast")) res = s->set_contrast(s, val);
    else if(!strcmp(variable, "brightness")) res = s->set_brightness(s, val);
    else if(!strcmp(variable, "saturation")) res = s->set_saturation(s, val);
    else if(!strcmp(variable, "gainceiling")) res = s->set_gainceiling(s, (gainceiling_t)val);
    else if(!strcmp(variable, "colorbar")) res = s->set_colorbar(s, val);
    else if(!strcmp(variable, "awb")) res = s->set_whitebal(s, val);
    else if(!strcmp(variable, "agc")) res = s->set_gain_ctrl(s, val);
    else if(!strcmp(variable, "aec")) res = s->set_exposure_ctrl(s, val);
    else if(!strcmp(variable, "hmirror")) res = s->set_hmirror(s, val);
    else if(!strcmp(variable, "vflip")) res = s->set_vflip(s, val);
    else if(!strcmp(variable, "awb_gain")) res = s->set_awb_gain(s, val);
    else if(!strcmp(variable, "agc_gain")) res = s->set_agc_gain(s, val);
    else if(!strcmp(variable, "aec_value")) res = s->set_aec_value(s, val);
    else if(!strcmp(variable, "aec2")) res = s->set_aec2(s, val);
    else if(!strcmp(variable, "dcw")) res = s->set_dcw(s, val);
    else if(!strcmp(variable, "bpc")) res = s->set_bpc(s, val);
    else if(!strcmp(variable, "wpc")) res = s->set_wpc(s, val);
    else if(!strcmp(variable, "raw_gma")) res = s->set_raw_gma(s, val);
    else if(!strcmp(variable, "lenc")) res = s->set_lenc(s, val);
    else if(!strcmp(variable, "special_effect")) res = s->set_special_effect(s, val);
    else if(!strcmp(variable, "wb_mode")) res = s->set_wb_mode(s, val);
    else if(!strcmp(variable, "ae_level")) res = s->set_ae_level(s, val);
    else {
        res = -1;
    }

    if(res){
        return httpd_resp_send_500(req);
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t status_handler(httpd_req_t *req){
    static char json_response[1024];

    sensor_t * s = esp_camera_sensor_get();
    char * p = json_response;
    *p++ = '{';

    p+=sprintf(p, "\"framesize\":%u,", s->status.framesize);
    p+=sprintf(p, "\"quality\":%u,", s->status.quality);
    p+=sprintf(p, "\"brightness\":%d,", s->status.brightness);
    p+=sprintf(p, "\"contrast\":%d,", s->status.contrast);
    p+=sprintf(p, "\"saturation\":%d,", s->status.saturation);
    p+=sprintf(p, "\"sharpness\":%d,", s->status.sharpness);
    p+=sprintf(p, "\"special_effect\":%u,", s->status.special_effect);
    p+=sprintf(p, "\"wb_mode\":%u,", s->status.wb_mode);
    p+=sprintf(p, "\"awb\":%u,", s->status.awb);
    p+=sprintf(p, "\"awb_gain\":%u,", s->status.awb_gain);
    p+=sprintf(p, "\"aec\":%u,", s->status.aec);
    p+=sprintf(p, "\"aec2\":%u,", s->status.aec2);
    p+=sprintf(p, "\"ae_level\":%d,", s->status.ae_level);
    p+=sprintf(p, "\"aec_value\":%u,", s->status.aec_value);
    p+=sprintf(p, "\"agc\":%u,", s->status.agc);
    p+=sprintf(p, "\"agc_gain\":%u,", s->status.agc_gain);
    p+=sprintf(p, "\"gainceiling\":%u,", s->status.gainceiling);
    p+=sprintf(p, "\"bpc\":%u,", s->status.bpc);
    p+=sprintf(p, "\"wpc\":%u,", s->status.wpc);
    p+=sprintf(p, "\"raw_gma\":%u,", s->status.raw_gma);
    p+=sprintf(p, "\"lenc\":%u,", s->status.lenc);
    p+=sprintf(p, "\"vflip\":%u,", s->status.vflip);
    p+=sprintf(p, "\"hmirror\":%u,", s->status.hmirror);
    p+=sprintf(p, "\"dcw\":%u,", s->status.dcw);
    p+=sprintf(p, "\"colorbar\":%u", s->status.colorbar);

    *p++ = '}';
    *p++ = 0;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json_response, strlen(json_response));
}

static esp_err_t index_handler(httpd_req_t *req){
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    return httpd_resp_send(req, (const char *)index_ov2640_html_gz, index_ov2640_html_gz_len);
}

// Handler for setting digital output
static esp_err_t gpio_do_handler(httpd_req_t *req)
{
  char query[256];
  char param[32];
  
  // Get query parameters
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }
  
  // Debug output
  Serial.print("GPIO do query: ");
  Serial.println(query);
  
  // Extract pin
  if (httpd_query_key_value(query, "pin", param, sizeof(param)) != ESP_OK) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    char response[128];
    sprintf(response, "{\"error\":\"Missing pin parameter\",\"success\":false}");
    return httpd_resp_send(req, response, strlen(response));
  }
  
  int pin = atoi(param);
  
  // Check if pin is safe to use
  if (!is_pin_safe(pin)) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    char response[128];
    sprintf(response, "{\"error\":\"Pin not safe to use\",\"success\":false}");
    return httpd_resp_send(req, response, strlen(response));
  }
  
  // Extract state
  if (httpd_query_key_value(query, "state", param, sizeof(param)) != ESP_OK) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    char response[128];
    sprintf(response, "{\"error\":\"Missing state parameter\",\"success\":false}");
    return httpd_resp_send(req, response, strlen(response));
  }
  
  // Initialize pin if not already initialized
  if (!do_pins_initialized[pin]) {
    pinMode(pin, OUTPUT);
    do_pins_initialized[pin] = true;
    
    // If this pin was previously used for PWM, release the channel
    if (ao_pins_initialized[pin]) {
      ledcDetach(pin);
      ao_pins_initialized[pin] = false;
    }
  }
  
  // Set pin state
  bool state = false;
  if (strcmp(param, "high") == 0 || strcmp(param, "1") == 0 || strcmp(param, "true") == 0) {
    state = true;
    digitalWrite(pin, HIGH);
  } else {
    digitalWrite(pin, LOW);
  }
  
  // Send response
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  char response[128];
  sprintf(response, "{\"pin\":%d,\"state\":\"%s\",\"success\":true}", pin, state ? "high" : "low");
  return httpd_resp_send(req, response, strlen(response));
}

// Handler for reading analog input
static esp_err_t gpio_ai_read_handler(httpd_req_t *req)
{
  char query[256];
  char param[32];
  
  // Get query parameters
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }
  
  // Extract pin
  if (httpd_query_key_value(query, "pin", param, sizeof(param)) != ESP_OK) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    char response[128];
    sprintf(response, "{\"error\":\"Missing pin parameter\",\"success\":false}");
    return httpd_resp_send(req, response, strlen(response));
  }
  
  int pin = atoi(param);
  
  // Check if pin is safe to use for analog input
  if (!is_pin_safe_ai(pin)) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    char response[128];
    sprintf(response, "{\"error\":\"Pin not safe for analog input\",\"success\":false}");
    return httpd_resp_send(req, response, strlen(response));
  }
  
  // Initialize pin if not already initialized
  if (!ai_pins_initialized[pin]) {
    pinMode(pin, INPUT);
    ai_pins_initialized[pin] = true;
  }
  
  // Read analog value
  int value = analogRead(pin);
  
  // Send response
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  char response[128];
  sprintf(response, "{\"pin\":%d,\"value\":%d,\"success\":true}", pin, value);
  return httpd_resp_send(req, response, strlen(response));
}

// Handler for setting analog output (PWM)
static esp_err_t gpio_ao_set_handler(httpd_req_t *req)
{
  char query[256];
  char param[32];
  
  // Get query parameters
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }
  
  // Extract pin
  if (httpd_query_key_value(query, "pin", param, sizeof(param)) != ESP_OK) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    char response[128];
    sprintf(response, "{\"error\":\"Missing pin parameter\",\"success\":false}");
    return httpd_resp_send(req, response, strlen(response));
  }
  
  int pin = atoi(param);
  
  // Check if pin is safe to use for analog output
  if (!is_pin_safe_ao(pin)) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    char response[128];
    sprintf(response, "{\"error\":\"Pin not safe for analog output\",\"success\":false}");
    return httpd_resp_send(req, response, strlen(response));
  }
  
  // Extract value
  if (httpd_query_key_value(query, "value", param, sizeof(param)) != ESP_OK) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    char response[128];
    sprintf(response, "{\"error\":\"Missing value parameter\",\"success\":false}");
    return httpd_resp_send(req, response, strlen(response));
  }
  
  int value = atoi(param);
  if (value < 0) value = 0;
  if (value > 255) value = 255;
  
  // Initialize pin if not already initialized
  if (!ao_pins_initialized[pin]) {
    // If this pin was previously used for digital output, we need to reconfigure
    if (do_pins_initialized[pin]) {
      do_pins_initialized[pin] = false;
    }
    
    // Configure PWM
    ledcAttach(pin, 5000, 8); // 5kHz, 8-bit resolution
    ao_pins_initialized[pin] = true;
  }
  
  // Set PWM value
  ledcWrite(pin, value);
  
  // Send response
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  char response[128];
  sprintf(response, "{\"pin\":%d,\"value\":%d,\"success\":true}", pin, value);
  return httpd_resp_send(req, response, strlen(response));
}

// Handler for setting multiple digital outputs
static esp_err_t gpio_do_all_handler(httpd_req_t *req)
{
  char query[256];
  char pins_str[128];
  char states_str[128];
  
  // Get query parameters
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }
  
  // Debug output
  Serial.print("GPIO do_all query: ");
  Serial.println(query);
  
  // Extract pins
  if (httpd_query_key_value(query, "pins", pins_str, sizeof(pins_str)) != ESP_OK) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    char response[128];
    sprintf(response, "{\"error\":\"Missing pins parameter\",\"success\":false}");
    return httpd_resp_send(req, response, strlen(response));
  }
  
  // Extract states
  if (httpd_query_key_value(query, "states", states_str, sizeof(states_str)) != ESP_OK) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    char response[128];
    sprintf(response, "{\"error\":\"Missing states parameter\",\"success\":false}");
    return httpd_resp_send(req, response, strlen(response));
  }
  
  // Parse pins and states
  char *pins_token = strtok(pins_str, ",");
  char *states_token = strtok(states_str, ",");
  
  // Prepare response
  char response[512] = "{\"results\":[";
  bool first = true;
  
  // Process each pin and state
  while (pins_token != NULL && states_token != NULL) {
    int pin = atoi(pins_token);
    bool state = false;
    
    // Check if pin is safe to use
    if (is_pin_safe(pin)) {
      // Initialize pin if not already initialized
      if (!do_pins_initialized[pin]) {
        pinMode(pin, OUTPUT);
        do_pins_initialized[pin] = true;
        
        // If this pin was previously used for PWM, release the channel
        if (ao_pins_initialized[pin]) {
          ledcDetach(pin);
          ao_pins_initialized[pin] = false;
        }
      }
      
      // Set pin state
      if (strcmp(states_token, "high") == 0 || strcmp(states_token, "1") == 0 || strcmp(states_token, "true") == 0) {
        state = true;
        digitalWrite(pin, HIGH);
      } else {
        digitalWrite(pin, LOW);
      }
      
      // Add to response
      char pin_result[128];
      sprintf(pin_result, "%s{\"pin\":%d,\"state\":\"%s\",\"success\":true}", 
              first ? "" : ",", pin, state ? "high" : "low");
      strcat(response, pin_result);
      first = false;
    } else {
      // Pin not safe, add error to response
      char pin_result[128];
      sprintf(pin_result, "%s{\"pin\":%d,\"error\":\"Pin not safe to use\",\"success\":false}", 
              first ? "" : ",", pin);
      strcat(response, pin_result);
      first = false;
    }
    
    // Get next tokens
    pins_token = strtok(NULL, ",");
    states_token = strtok(NULL, ",");
  }
  
  // Complete response
  strcat(response, "]}");
  
  // Send response
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, response, strlen(response));
}

// Handler for getting GPIO overview
static esp_err_t gpio_overview_handler(httpd_req_t *req)
{
  // Prepare response
  char response[1024] = "{";
  
  // Add safe digital output pins
  strcat(response, "\"safe_do_pins\":[");
  for (int i = 0; i < sizeof(safe_do_pins) / sizeof(safe_do_pins[0]); i++) {
    char pin_str[8];
    sprintf(pin_str, "%s%d", i > 0 ? "," : "", safe_do_pins[i]);
    strcat(response, pin_str);
  }
  strcat(response, "],");
  
  // Add safe digital input pins
  strcat(response, "\"safe_di_pins\":[");
  for (int i = 0; i < sizeof(safe_di_pins) / sizeof(safe_di_pins[0]); i++) {
    char pin_str[8];
    sprintf(pin_str, "%s%d", i > 0 ? "," : "", safe_di_pins[i]);
    strcat(response, pin_str);
  }
  strcat(response, "],");
  
  // Add safe analog output pins
  strcat(response, "\"analog_output_pins\":[");
  for (int i = 0; i < sizeof(analog_output_pins) / sizeof(analog_output_pins[0]); i++) {
    char pin_str[8];
    sprintf(pin_str, "%s%d", i > 0 ? "," : "", analog_output_pins[i]);
    strcat(response, pin_str);
  }
  strcat(response, "],");
  
  // Add safe analog input pins
  strcat(response, "\"analog_input_pins\":[");
  for (int i = 0; i < sizeof(analog_input_pins) / sizeof(analog_input_pins[0]); i++) {
    char pin_str[8];
    sprintf(pin_str, "%s%d", i > 0 ? "," : "", analog_input_pins[i]);
    strcat(response, pin_str);
  }
  strcat(response, "],");
  
  // Add initialized pins
  strcat(response, "\"initialized_pins\":{");
  bool first = true;
  
  // Digital output pins
  for (int i = 0; i < 50; i++) {
    if (do_pins_initialized[i]) {
      char pin_str[32];
      sprintf(pin_str, "%s\"%d\":\"digital_output\"", first ? "" : ",", i);
      strcat(response, pin_str);
      first = false;
    }
  }
  
  // Digital input pins
  for (int i = 0; i < 50; i++) {
    if (di_pins_initialized[i]) {
      char pin_str[32];
      sprintf(pin_str, "%s\"%d\":\"digital_input\"", first ? "" : ",", i);
      strcat(response, pin_str);
      first = false;
    }
  }
  
  // Analog output pins
  for (int i = 0; i < 50; i++) {
    if (ao_pins_initialized[i]) {
      char pin_str[32];
      sprintf(pin_str, "%s\"%d\":\"analog_output\"", first ? "" : ",", i);
      strcat(response, pin_str);
      first = false;
    }
  }
  
  // Analog input pins
  for (int i = 0; i < 50; i++) {
    if (ai_pins_initialized[i]) {
      char pin_str[32];
      sprintf(pin_str, "%s\"%d\":\"analog_input\"", first ? "" : ",", i);
      strcat(response, pin_str);
      first = false;
    }
  }
  
  strcat(response, "},");
  
  // Add pin states
  strcat(response, "\"pin_states\":{");
  first = true;
  
  // Digital output pins
  for (int i = 0; i < 50; i++) {
    if (do_pins_initialized[i]) {
      char pin_str[32];
      int state = digitalRead(i);
      sprintf(pin_str, "%s\"%d\":\"%s\"", first ? "" : ",", i, state ? "high" : "low");
      strcat(response, pin_str);
      first = false;
    }
  }
  
  // Analog output pins
  for (int i = 0; i < 50; i++) {
    if (ao_pins_initialized[i]) {
      char pin_str[32];
      // We don't have a direct way to read the current PWM value, so we just indicate it's PWM
      sprintf(pin_str, "%s\"%d\":\"pwm\"", first ? "" : ",", i);
      strcat(response, pin_str);
      first = false;
    }
  }
  
  // Analog input pins
  for (int i = 0; i < 50; i++) {
    if (ai_pins_initialized[i] && is_pin_safe_ai(i)) {
      char pin_str[32];
      int value = analogRead(i);
      sprintf(pin_str, "%s\"%d\":%d", first ? "" : ",", i, value);
      strcat(response, pin_str);
      first = false;
    }
  }
  
  strcat(response, "}}");
  
  // Send response
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, response, strlen(response));
}

void startCameraServer(){
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;

    httpd_handle_t camera_httpd = NULL;

    // Define URI handlers
    httpd_uri_t index_uri_def = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = index_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t status_uri_def = {
        .uri       = "/camera/status",
        .method    = HTTP_GET,
        .handler   = status_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t cmd_uri_def = {
        .uri       = "/camera/control",
        .method    = HTTP_GET,
        .handler   = cmd_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t capture_uri_def = {
        .uri       = "/camera/capture",
        .method    = HTTP_GET,
        .handler   = capture_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t stream_uri_def = {
        .uri       = "/camera/stream",
        .method    = HTTP_GET,
        .handler   = stream_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t gpio_do_uri_def = {
        .uri       = "/gpio/do",
        .method    = HTTP_GET,
        .handler   = gpio_do_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t gpio_ai_read_uri_def = {
        .uri       = "/gpio/ai/read",
        .method    = HTTP_GET,
        .handler   = gpio_ai_read_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t gpio_ao_set_uri_def = {
        .uri       = "/gpio/ao/set",
        .method    = HTTP_GET,
        .handler   = gpio_ao_set_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t gpio_do_all_uri_def = {
        .uri       = "/gpio/do/all",
        .method    = HTTP_GET,
        .handler   = gpio_do_all_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t gpio_overview_uri_def = {
        .uri       = "/gpio/overview",
        .method    = HTTP_GET,
        .handler   = gpio_overview_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t network_config_get_uri_def = {
        .uri       = "/network/config/get",
        .method    = HTTP_GET,
        .handler   = network_config_get_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t network_config_set_uri_def = {
        .uri       = "/network/config/set",
        .method    = HTTP_GET,
        .handler   = network_config_set_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t restart_uri_def = {
        .uri       = "/restart",
        .method    = HTTP_GET,
        .handler   = restart_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t neopixel_set_uri_def = {
        .uri       = "/neopixel/set",
        .method    = HTTP_GET,
        .handler   = neopixel_set_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t neopixel_off_uri_def = {
        .uri       = "/neopixel/off",
        .method    = HTTP_GET,
        .handler   = neopixel_off_handler,
        .user_ctx  = NULL
    };

    Serial.printf("Starting web server on port: '%d'\n", config.server_port);
    if (httpd_start(&camera_httpd, &config) == ESP_OK) {
        // Register URI handlers
        httpd_register_uri_handler(camera_httpd, &index_uri_def);
        httpd_register_uri_handler(camera_httpd, &cmd_uri_def);
        httpd_register_uri_handler(camera_httpd, &status_uri_def);
        httpd_register_uri_handler(camera_httpd, &capture_uri_def);
        httpd_register_uri_handler(camera_httpd, &stream_uri_def);
        
        // GPIO handlers
        httpd_register_uri_handler(camera_httpd, &gpio_do_uri_def);
        httpd_register_uri_handler(camera_httpd, &gpio_ai_read_uri_def);
        httpd_register_uri_handler(camera_httpd, &gpio_ao_set_uri_def);
        httpd_register_uri_handler(camera_httpd, &gpio_do_all_uri_def);
        httpd_register_uri_handler(camera_httpd, &gpio_overview_uri_def);
        
        // Network configuration handlers
        httpd_register_uri_handler(camera_httpd, &network_config_get_uri_def);
        httpd_register_uri_handler(camera_httpd, &network_config_set_uri_def);
        httpd_register_uri_handler(camera_httpd, &restart_uri_def);
        
        // NeoPixel handlers
        httpd_register_uri_handler(camera_httpd, &neopixel_set_uri_def);
        httpd_register_uri_handler(camera_httpd, &neopixel_off_uri_def);
    }

    // Initialize network configuration
    initNetworkConfig();
    
    // Initialize NeoPixel
    initNeoPixel();
    
    Serial.println("Camera Web Server Started");
}
