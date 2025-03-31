#include "esp_timer.h"
#include "esp_camera.h"
#include "img_converters.h"
#include "fb_gfx.h"
#include "esp32-hal-ledc.h"
#include "sdkconfig.h"
#include "camera_index.h"
#include <Arduino.h>
#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#endif
#include "utilities.h"
#include "network_config.h"
#include "neopixel.h"
#include "esp_http_server.h"

// Face Detection will not work on boards without (or with disabled) PSRAM
#ifdef BOARD_HAS_PSRAM
#define CONFIG_ESP_FACE_DETECT_ENABLED 0
// Face Recognition takes upward from 15 seconds per frame on chips other than ESP32S3
// Makes no sense to have it enabled for them
#if CONFIG_IDF_TARGET_ESP32S3
#define CONFIG_ESP_FACE_RECOGNITION_ENABLED 0
#else
#define CONFIG_ESP_FACE_RECOGNITION_ENABLED 0
#endif
#else
#define CONFIG_ESP_FACE_DETECT_ENABLED 0
#define CONFIG_ESP_FACE_RECOGNITION_ENABLED 0
#endif

#if CONFIG_ESP_FACE_DETECT_ENABLED

#include <vector>
#include "human_face_detect_msr01.hpp"
#include "human_face_detect_mnp01.hpp"

#define TWO_STAGE 1 /*<! 1: detect by two-stage which is more accurate but slower(with keypoints). */
/*<! 0: detect by one-stage which is less accurate but faster(without keypoints). */

#if TWO_STAGE
#define FACE_COLOR_WHITE 0x00FFFFFF
#define FACE_COLOR_BLACK 0x00000000
#define FACE_COLOR_RED 0x000000FF
#define FACE_COLOR_GREEN 0x0000FF00
#define FACE_COLOR_BLUE 0x00FF0000
#define FACE_COLOR_YELLOW (FACE_COLOR_RED | FACE_COLOR_GREEN)
#define FACE_COLOR_CYAN (FACE_COLOR_BLUE | FACE_COLOR_GREEN)
#define FACE_COLOR_PURPLE (FACE_COLOR_BLUE | FACE_COLOR_RED)
#endif

#endif

#define PART_BOUNDARY "123456789000000000000987654321"
static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;

#if CONFIG_ESP_FACE_DETECT_ENABLED
static int8_t detection_enabled = 0;
static int8_t recognition_enabled = 0;
static int8_t is_enrolling = 0;
#endif

// Array of safe digital output pins
const int safe_do_pins[] = {0, 4, 5, 6, 7, 16, 17, 19, 20, 21, 33, 34, 35, 36, 37, 43, 44};
const int num_safe_do_pins = sizeof(safe_do_pins) / sizeof(safe_do_pins[0]);

// Array of reserved pins (used by camera, Ethernet, etc.)
const int reserved_pins[] = {1, 2, 3, 8, 9, 10, 11, 12, 13, 14, 15, 18, 38, 39, 40, 41, 42, 45, 46, 47, 48};
const int num_reserved_pins = sizeof(reserved_pins) / sizeof(reserved_pins[0]);

// Array of analog input pins
const int analog_input_pins[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
const int num_analog_input_pins = sizeof(analog_input_pins) / sizeof(analog_input_pins[0]);

// Array of analog output pins (same as safe digital output pins)
const int analog_output_pins[] = {0, 4, 5, 6, 7, 16, 17, 19, 20, 21, 33, 34, 35, 36, 37, 43, 44};
const int num_analog_output_pins = sizeof(analog_output_pins) / sizeof(analog_output_pins[0]);

// Track which pins have been initialized
bool do_pins_initialized[50] = {false};
bool ao_pins_initialized[50] = {false};
int ao_pin_channels[50] = {-1};
int next_pwm_channel = 0;

// Check if a pin is safe to use
bool is_pin_safe(int pin) {
    for (int i = 0; i < num_safe_do_pins; i++) {
        if (pin == safe_do_pins[i]) {
            return true;
        }
    }
    return false;
}

// Check if a pin is valid for analog input
bool is_valid_ai_pin(int pin) {
    for (int i = 0; i < num_analog_input_pins; i++) {
        if (pin == analog_input_pins[i]) {
            return true;
        }
    }
    return false;
}

// Check if a pin is valid for analog output
bool is_valid_ao_pin(int pin) {
    for (int i = 0; i < num_analog_output_pins; i++) {
        if (pin == analog_output_pins[i]) {
            return true;
        }
    }
    return false;
}

typedef struct
{
    httpd_req_t *req;
    size_t len;
} jpg_chunking_t;

static size_t jpg_encode_stream(void *arg, size_t index, const void *data, size_t len)
{
    jpg_chunking_t *j = (jpg_chunking_t *)arg;
    if (!index)
    {
        j->len = 0;
    }
    if (httpd_resp_send_chunk(j->req, (const char *)data, len) != ESP_OK)
    {
        return 0;
    }
    j->len += len;
    return len;
}

static esp_err_t capture_handler(httpd_req_t *req)
{
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    int64_t fr_start = esp_timer_get_time();

    fb = esp_camera_fb_get();
    if (!fb)
    {
        Serial.println("Camera capture failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

#if CONFIG_ESP_FACE_DETECT_ENABLED
    size_t out_len, out_width, out_height;
    uint8_t *out_buf;
    bool s;
    bool detected = false;
    int face_id = 0;
    if (!detection_enabled || fb->width > 400)
    {
#endif
        size_t fb_len = 0;
        if (fb->format == PIXFORMAT_JPEG)
        {
            fb_len = fb->len;
            res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
        }
        else
        {
            jpg_chunking_t jchunk = {req, 0};
            res = frame2jpg_cb(fb, 80, jpg_encode_stream, &jchunk) ? ESP_OK : ESP_FAIL;
            httpd_resp_send_chunk(req, NULL, 0);
            fb_len = jchunk.len;
        }
        esp_camera_fb_return(fb);
        int64_t fr_end = esp_timer_get_time();
        Serial.printf("JPG: %uB %ums\n", (uint32_t)(fb_len), (uint32_t)((fr_end - fr_start) / 1000));
        return res;
#if CONFIG_ESP_FACE_DETECT_ENABLED
    }

    int64_t fr_ready = esp_timer_get_time();
    int64_t fr_face = fr_ready;
    int64_t fr_recognize = fr_ready;
    int64_t fr_encode = fr_ready;

    detected = true;
    face_id = 0;

    fr_encode = esp_timer_get_time();
    out_len = fb->width * fb->height * 3;
    out_width = fb->width;
    out_height = fb->height;

    out_buf = (uint8_t *)malloc(out_len);
    if (!out_buf)
    {
        Serial.println("out_buf malloc failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    s = fmt2rgb888(fb->buf, fb->len, fb->format, out_buf);
    esp_camera_fb_return(fb);
    if (!s)
    {
        free(out_buf);
        Serial.println("to rgb888 failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    jpg_chunking_t jchunk = {req, 0};
    s = fmt2jpg_cb(out_buf, out_len, out_width, out_height, PIXFORMAT_RGB888, 90, jpg_encode_stream, &jchunk);
    free(out_buf);
    if (!s)
    {
        Serial.println("JPEG compression failed");
        return ESP_FAIL;
    }

    int64_t fr_end = esp_timer_get_time();
    Serial.printf("FACE: %uB %ums %s%d\n", (uint32_t)(jchunk.len), (uint32_t)((fr_end - fr_start) / 1000), detected ? "DETECTED " : "", face_id);
    return res;
#endif
}

static esp_err_t stream_handler(httpd_req_t *req)
{
    camera_fb_t *fb = NULL;
    struct timeval _timestamp;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t *_jpg_buf = NULL;
    char *part_buf[128];
#if CONFIG_ESP_FACE_DETECT_ENABLED
    dl_matrix3du_t *image_matrix = NULL;
    bool detected = false;
    int face_id = 0;
    int64_t fr_start = 0;
    int64_t fr_ready = 0;
    int64_t fr_face = 0;
    int64_t fr_recognize = 0;
    int64_t fr_encode = 0;
    int64_t ready_time = 0;
    int64_t face_time = 0;
    int64_t recognize_time = 0;
    int64_t encode_time = 0;
    int64_t process_time = 0;
#endif

    static int64_t last_frame = 0;
    if (!last_frame)
    {
        last_frame = esp_timer_get_time();
    }

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK)
    {
        return res;
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "X-Framerate", "60");

#if CONFIG_ESP_FACE_DETECT_ENABLED
    detection_enabled = 0;
    recognition_enabled = 0;
#endif

    while (true)
    {
#if CONFIG_ESP_FACE_DETECT_ENABLED
        detected = false;
        face_id = 0;
#endif

        fb = esp_camera_fb_get();
        if (!fb)
        {
            Serial.println("Camera capture failed");
            res = ESP_FAIL;
        }
        else
        {
            _timestamp.tv_sec = fb->timestamp.tv_sec;
            _timestamp.tv_usec = fb->timestamp.tv_usec;
#if CONFIG_ESP_FACE_DETECT_ENABLED
            fr_start = esp_timer_get_time();
            fr_ready = fr_start;
            fr_face = fr_start;
            fr_encode = fr_start;
            fr_recognize = fr_start;
            if (fb->width > 400)
            {
#endif
                if (fb->format != PIXFORMAT_JPEG)
                {
                    bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
                    esp_camera_fb_return(fb);
                    fb = NULL;
                    if (!jpeg_converted)
                    {
                        Serial.println("JPEG compression failed");
                        res = ESP_FAIL;
                    }
                }
                else
                {
                    _jpg_buf_len = fb->len;
                    _jpg_buf = fb->buf;
                }
#if CONFIG_ESP_FACE_DETECT_ENABLED
            }
            else
            {
                image_matrix = dl_matrix3du_alloc(1, fb->width, fb->height, 3);

                if (!image_matrix)
                {
                    Serial.println("dl_matrix3du_alloc failed");
                    res = ESP_FAIL;
                }
                else
                {
                    if (!fmt2rgb888(fb->buf, fb->len, fb->format, image_matrix->item))
                    {
                        Serial.println("fmt2rgb888 failed");
                        res = ESP_FAIL;
                    }
                    else
                    {
                        fr_ready = esp_timer_get_time();
                        ready_time = (fr_ready - fr_start) / 1000;
                        if (detection_enabled)
                        {
                            fr_face = esp_timer_get_time();
                            face_time = fr_face - fr_ready;
                            fr_recognize = fr_face;
                        }
                        if (fb->format != PIXFORMAT_JPEG)
                        {
                            if (!fmt2jpg(image_matrix->item, fb->width * fb->height * 3, fb->width, fb->height, PIXFORMAT_RGB888, 90, &_jpg_buf, &_jpg_buf_len))
                            {
                                Serial.println("fmt2jpg failed");
                                res = ESP_FAIL;
                            }
                            esp_camera_fb_return(fb);
                            fb = NULL;
                        }
                        else
                        {
                            _jpg_buf = fb->buf;
                            _jpg_buf_len = fb->len;
                        }
                        fr_encode = esp_timer_get_time();
                        encode_time = (fr_encode - fr_recognize) / 1000;
                        process_time = (fr_encode - fr_start) / 1000;
                    }
                    dl_matrix3du_free(image_matrix);
                }
            }
#endif
        }
        if (res == ESP_OK)
        {
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }
        if (res == ESP_OK)
        {
            size_t hlen = snprintf((char *)part_buf, 128, _STREAM_PART, _jpg_buf_len);
            res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        }
        if (res == ESP_OK)
        {
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }
        if (fb)
        {
            esp_camera_fb_return(fb);
            fb = NULL;
            _jpg_buf = NULL;
        }
        else if (_jpg_buf)
        {
            free(_jpg_buf);
            _jpg_buf = NULL;
        }
        if (res != ESP_OK)
        {
            break;
        }
        int64_t fr_end = esp_timer_get_time();

#if CONFIG_ESP_FACE_DETECT_ENABLED
        int64_t frame_time = fr_end - last_frame;
        last_frame = fr_end;
        frame_time /= 1000;
        uint32_t avg_frame_time = ra_filter_run(&ra_filter, frame_time);
        Serial.printf("MJPG: %uB %ums (%.1ffps), AVG: %ums (%.1ffps), %u+%u+%u+%u=%u %s%d\n",
                      (uint32_t)(_jpg_buf_len),
                      (uint32_t)frame_time, 1000.0 / (uint32_t)frame_time,
                      avg_frame_time, 1000.0 / avg_frame_time,
                      (uint32_t)ready_time, (uint32_t)face_time, (uint32_t)recognize_time, (uint32_t)encode_time, (uint32_t)process_time,
                      (detected) ? "DETECTED " : "", face_id);
#else
        int64_t frame_time = fr_end - last_frame;
        last_frame = fr_end;
        frame_time /= 1000;
        Serial.printf("MJPG: %uB %ums (%.1ffps)\n",
                      (uint32_t)(_jpg_buf_len),
                      (uint32_t)frame_time, 1000.0 / (uint32_t)frame_time);
#endif
    }

    last_frame = 0;
    return res;
}

static esp_err_t cmd_handler(httpd_req_t *req)
{
    char *buf;
    size_t buf_len;
    char variable[32] = {
        0,
    };
    char value[32] = {
        0,
    };

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1)
    {
        buf = (char *)malloc(buf_len);
        if (!buf)
        {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
        {
            if (httpd_query_key_value(buf, "var", variable, sizeof(variable)) == ESP_OK &&
                httpd_query_key_value(buf, "val", value, sizeof(value)) == ESP_OK)
            {
            }
            else
            {
                free(buf);
                httpd_resp_send_404(req);
                return ESP_FAIL;
            }
        }
        else
        {
            free(buf);
            httpd_resp_send_404(req);
            return ESP_FAIL;
        }
        free(buf);
    }
    else
    {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    int val = atoi(value);
    sensor_t *s = esp_camera_sensor_get();
    int res = 0;

    if (!strcmp(variable, "framesize"))
    {
        if (s->pixformat == PIXFORMAT_JPEG)
            res = s->set_framesize(s, (framesize_t)val);
    }
    else if (!strcmp(variable, "quality"))
        res = s->set_quality(s, val);
    else if (!strcmp(variable, "contrast"))
        res = s->set_contrast(s, val);
    else if (!strcmp(variable, "brightness"))
        res = s->set_brightness(s, val);
    else if (!strcmp(variable, "saturation"))
        res = s->set_saturation(s, val);
    else if (!strcmp(variable, "gainceiling"))
        res = s->set_gainceiling(s, (gainceiling_t)val);
    else if (!strcmp(variable, "colorbar"))
        res = s->set_colorbar(s, val);
    else if (!strcmp(variable, "awb"))
        res = s->set_whitebal(s, val);
    else if (!strcmp(variable, "agc"))
        res = s->set_gain_ctrl(s, val);
    else if (!strcmp(variable, "aec"))
        res = s->set_exposure_ctrl(s, val);
    else if (!strcmp(variable, "hmirror"))
        res = s->set_hmirror(s, val);
    else if (!strcmp(variable, "vflip"))
        res = s->set_vflip(s, val);
    else if (!strcmp(variable, "awb_gain"))
        res = s->set_awb_gain(s, val);
    else if (!strcmp(variable, "agc_gain"))
        res = s->set_agc_gain(s, val);
    else if (!strcmp(variable, "aec_value"))
        res = s->set_aec_value(s, val);
    else if (!strcmp(variable, "aec2"))
        res = s->set_aec2(s, val);
    else if (!strcmp(variable, "dcw"))
        res = s->set_dcw(s, val);
    else if (!strcmp(variable, "bpc"))
        res = s->set_bpc(s, val);
    else if (!strcmp(variable, "wpc"))
        res = s->set_wpc(s, val);
    else if (!strcmp(variable, "raw_gma"))
        res = s->set_raw_gma(s, val);
    else if (!strcmp(variable, "lenc"))
        res = s->set_lenc(s, val);
    else if (!strcmp(variable, "special_effect"))
        res = s->set_special_effect(s, val);
    else if (!strcmp(variable, "wb_mode"))
        res = s->set_wb_mode(s, val);
    else if (!strcmp(variable, "ae_level"))
        res = s->set_ae_level(s, val);
#if CONFIG_ESP_FACE_DETECT_ENABLED
    else if (!strcmp(variable, "face_detect"))
    {
        detection_enabled = val;
#if CONFIG_ESP_FACE_RECOGNITION_ENABLED
        if (!detection_enabled)
        {
            recognition_enabled = 0;
        }
#endif
    }
#if CONFIG_ESP_FACE_RECOGNITION_ENABLED
    else if (!strcmp(variable, "face_enroll"))
        is_enrolling = val;
    else if (!strcmp(variable, "face_recognize"))
    {
        recognition_enabled = val;
        if (recognition_enabled)
        {
            detection_enabled = val;
        }
    }
#endif
#endif
    else if (!strcmp(variable, "ir_led"))
    {
        if (val)
        {
            digitalWrite(IR_FILTER_NUM, HIGH);
        }
        else
        {
            digitalWrite(IR_FILTER_NUM, LOW);
        }
    }
    else
    {
        res = -1;
    }

    if (res)
    {
        return httpd_resp_send_500(req);
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t status_handler(httpd_req_t *req)
{
    static char json_response[1024];

    sensor_t *s = esp_camera_sensor_get();
    char *p = json_response;
    *p++ = '{';

    p += sprintf(p, "\"framesize\":%u,", s->status.framesize);
    p += sprintf(p, "\"quality\":%u,", s->status.quality);
    p += sprintf(p, "\"brightness\":%d,", s->status.brightness);
    p += sprintf(p, "\"contrast\":%d,", s->status.contrast);
    p += sprintf(p, "\"saturation\":%d,", s->status.saturation);
    p += sprintf(p, "\"sharpness\":%d,", s->status.sharpness);
    p += sprintf(p, "\"special_effect\":%u,", s->status.special_effect);
    p += sprintf(p, "\"wb_mode\":%u,", s->status.wb_mode);
    p += sprintf(p, "\"awb\":%u,", s->status.awb);
    p += sprintf(p, "\"awb_gain\":%u,", s->status.awb_gain);
    p += sprintf(p, "\"aec\":%u,", s->status.aec);
    p += sprintf(p, "\"aec2\":%u,", s->status.aec2);
    p += sprintf(p, "\"ae_level\":%d,", s->status.ae_level);
    p += sprintf(p, "\"aec_value\":%u,", s->status.aec_value);
    p += sprintf(p, "\"agc\":%u,", s->status.agc);
    p += sprintf(p, "\"agc_gain\":%u,", s->status.agc_gain);
    p += sprintf(p, "\"gainceiling\":%u,", s->status.gainceiling);
    p += sprintf(p, "\"bpc\":%u,", s->status.bpc);
    p += sprintf(p, "\"wpc\":%u,", s->status.wpc);
    p += sprintf(p, "\"raw_gma\":%u,", s->status.raw_gma);
    p += sprintf(p, "\"lenc\":%u,", s->status.lenc);
    p += sprintf(p, "\"vflip\":%u,", s->status.vflip);
    p += sprintf(p, "\"hmirror\":%u,", s->status.hmirror);
    p += sprintf(p, "\"dcw\":%u,", s->status.dcw);
    p += sprintf(p, "\"colorbar\":%u,", s->status.colorbar);
#if CONFIG_ESP_FACE_DETECT_ENABLED
    p += sprintf(p, "\"face_detect\":%u,", detection_enabled);
#if CONFIG_ESP_FACE_RECOGNITION_ENABLED
    p += sprintf(p, "\"face_enroll\":%u,", is_enrolling);
    p += sprintf(p, "\"face_recognize\":%u,", recognition_enabled);
#endif
#endif
    p += sprintf(p, "\"ir_led\":%u", digitalRead(IR_FILTER_NUM));
    *p++ = '}';
    *p++ = 0;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json_response, strlen(json_response));
}

static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    sensor_t *s = esp_camera_sensor_get();
    if (s != NULL)
    {
        if (s->id.PID == OV3660_PID)
        {
            return httpd_resp_send(req, (const char *)index_ov2640_html_gz, index_ov2640_html_gz_len);
        }
        else if (s->id.PID == OV5640_PID)
        {
            return httpd_resp_send(req, (const char *)index_ov2640_html_gz, index_ov2640_html_gz_len);
        }
        else
        {
            return httpd_resp_send(req, (const char *)index_ov2640_html_gz, index_ov2640_html_gz_len);
        }
    }
    else
    {
        Serial.println("Camera sensor not found");
        return httpd_resp_send_500(req);
    }
}

// Handler for digital output control
static esp_err_t gpio_do_handler(httpd_req_t *req)
{
    char query[256];
    char pin_str[32];
    char state_str[32];
    
    // Get query parameters
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    
    // Extract pin number
    if (httpd_query_key_value(query, "pin", pin_str, sizeof(pin_str)) != ESP_OK) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        char response[128];
        sprintf(response, "{\"error\":\"Missing pin parameter\",\"success\":false}");
        return httpd_resp_send(req, response, strlen(response));
    }
    
    int pin = atoi(pin_str);
    
    // Check if pin is safe to use
    if (!is_pin_safe(pin)) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        char response[128];
        sprintf(response, "{\"error\":\"Pin %d is not safe to use\",\"success\":false}", pin);
        return httpd_resp_send(req, response, strlen(response));
    }
    
    // Extract state
    if (httpd_query_key_value(query, "state", state_str, sizeof(state_str)) != ESP_OK) {
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
    if (strcmp(state_str, "high") == 0 || strcmp(state_str, "1") == 0) {
        digitalWrite(pin, HIGH);
    } else if (strcmp(state_str, "low") == 0 || strcmp(state_str, "0") == 0) {
        digitalWrite(pin, LOW);
    } else {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        char response[128];
        sprintf(response, "{\"error\":\"Invalid state (use 'high' or 'low')\",\"success\":false}");
        return httpd_resp_send(req, response, strlen(response));
    }
    
    // Send response
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    char response[64];
    sprintf(response, "{\"pin\":%d,\"state\":\"%s\",\"success\":true}", pin, state_str);
    return httpd_resp_send(req, response, strlen(response));
}

// Handler for analog input reading
static esp_err_t gpio_ai_read_handler(httpd_req_t *req)
{
    char query[256];
    char pin_str[32];
    
    // Get query parameters
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    
    // Extract pin number
    if (httpd_query_key_value(query, "pin", pin_str, sizeof(pin_str)) != ESP_OK) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        char response[128];
        sprintf(response, "{\"error\":\"Missing pin parameter\",\"success\":false}");
        return httpd_resp_send(req, response, strlen(response));
    }
    
    int pin = atoi(pin_str);
    
    // Check if pin is valid for analog input
    if (!is_valid_ai_pin(pin)) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        char response[128];
        sprintf(response, "{\"error\":\"Pin %d is not valid for analog input\",\"success\":false}", pin);
        return httpd_resp_send(req, response, strlen(response));
    }
    
    // Read analog value
    int value = analogRead(pin);
    
    // Send response
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    char response[64];
    sprintf(response, "{\"pin\":%d,\"value\":%d,\"success\":true}", pin, value);
    return httpd_resp_send(req, response, strlen(response));
}

// Handler for analog output setting
static esp_err_t gpio_ao_set_handler(httpd_req_t *req)
{
    char query[256];
    char pin_str[32];
    char value_str[32];
    
    // Get query parameters
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    
    // Extract pin number
    if (httpd_query_key_value(query, "pin", pin_str, sizeof(pin_str)) != ESP_OK) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        char response[128];
        sprintf(response, "{\"error\":\"Missing pin parameter\",\"success\":false}");
        return httpd_resp_send(req, response, strlen(response));
    }
    
    int pin = atoi(pin_str);
    
    // Check if pin is valid for analog output
    if (!is_valid_ao_pin(pin)) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        char response[128];
        sprintf(response, "{\"error\":\"Pin %d is not valid for analog output\",\"success\":false}", pin);
        return httpd_resp_send(req, response, strlen(response));
    }
    
    // Extract value
    if (httpd_query_key_value(query, "value", value_str, sizeof(value_str)) != ESP_OK) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        char response[128];
        sprintf(response, "{\"error\":\"Missing value parameter\",\"success\":false}");
        return httpd_resp_send(req, response, strlen(response));
    }
    
    int value = atoi(value_str);
    if (value < 0) value = 0;
    if (value > 255) value = 255;
    
    // Initialize pin for PWM if not already initialized
    if (!ao_pins_initialized[pin]) {
        // If this pin was previously used for digital output, we need to reconfigure
        if (do_pins_initialized[pin]) {
            do_pins_initialized[pin] = false;
        }
        
        // Assign a PWM channel to this pin
        int channel = next_pwm_channel++;
        if (next_pwm_channel >= 16) next_pwm_channel = 0; // ESP32 has 16 PWM channels
        
        // Updated to use the correct parameters for ledcAttach
        uint32_t freq = 5000; // 5kHz
        uint8_t resolution = 8; // 8-bit resolution
        ledcAttach(pin, freq, resolution);
        ao_pin_channels[pin] = channel;
        ao_pins_initialized[pin] = true;
    }
    
    // Set PWM value
    ledcWrite(ao_pin_channels[pin], value);
    
    // Send response
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    char response[64];
    sprintf(response, "{\"pin\":%d,\"value\":%d,\"success\":true}", pin, value);
    return httpd_resp_send(req, response, strlen(response));
}

// Handler for setting multiple digital outputs at once
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
    char *pin_token = strtok(pins_str, ",");
    char *state_token = strtok(states_str, ",");
    
    char response[512] = "{\"results\":[";
    bool first = true;
    
    while (pin_token != NULL && state_token != NULL) {
        int pin = atoi(pin_token);
        
        // Check if pin is safe to use
        if (!is_pin_safe(pin)) {
            if (!first) strcat(response, ",");
            char result[128];
            sprintf(result, "{\"pin\":%d,\"error\":\"Pin not safe to use\",\"success\":false}", pin);
            strcat(response, result);
        } else {
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
            if (strcmp(state_token, "high") == 0 || strcmp(state_token, "1") == 0) {
                digitalWrite(pin, HIGH);
                if (!first) strcat(response, ",");
                char result[64];
                sprintf(result, "{\"pin\":%d,\"state\":\"high\",\"success\":true}", pin);
                strcat(response, result);
            } else if (strcmp(state_token, "low") == 0 || strcmp(state_token, "0") == 0) {
                digitalWrite(pin, LOW);
                if (!first) strcat(response, ",");
                char result[64];
                sprintf(result, "{\"pin\":%d,\"state\":\"low\",\"success\":true}", pin);
                strcat(response, result);
            } else {
                if (!first) strcat(response, ",");
                char result[128];
                sprintf(result, "{\"pin\":%d,\"error\":\"Invalid state\",\"success\":false}", pin);
                strcat(response, result);
            }
        }
        
        first = false;
        pin_token = strtok(NULL, ",");
        state_token = strtok(NULL, ",");
    }
    
    strcat(response, "]}");
    
    // Send response
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, response, strlen(response));
}

// Handler for getting GPIO overview
static esp_err_t gpio_overview_handler(httpd_req_t *req)
{
    char response[1024];
    char *p = response;
    
    p += sprintf(p, "{\"safe_do_pins\":[");
    for (int i = 0; i < num_safe_do_pins; i++) {
        p += sprintf(p, "%d%s", safe_do_pins[i], (i < num_safe_do_pins - 1) ? "," : "");
    }
    
    p += sprintf(p, "],\"reserved_pins\":[");
    for (int i = 0; i < num_reserved_pins; i++) {
        p += sprintf(p, "%d%s", reserved_pins[i], (i < num_reserved_pins - 1) ? "," : "");
    }
    
    p += sprintf(p, "],\"analog_input_pins\":[");
    for (int i = 0; i < num_analog_input_pins; i++) {
        p += sprintf(p, "%d%s", analog_input_pins[i], (i < num_analog_input_pins - 1) ? "," : "");
    }
    
    p += sprintf(p, "],\"analog_output_pins\":[");
    for (int i = 0; i < num_analog_output_pins; i++) {
        p += sprintf(p, "%d%s", analog_output_pins[i], (i < num_analog_output_pins - 1) ? "," : "");
    }
    
    p += sprintf(p, "],\"initialized_pins\":{");
    bool first = true;
    for (int i = 0; i < 50; i++) {
        if (do_pins_initialized[i] || ao_pins_initialized[i]) {
            if (!first) p += sprintf(p, ",");
            p += sprintf(p, "\"%d\":\"%s\"", i, ao_pins_initialized[i] ? "analog" : "digital");
            first = false;
        }
    }
    
    p += sprintf(p, "}}");
    
    // Send response
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, response, strlen(response));
}

static esp_err_t bmp_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "image/bmp");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.bmp");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    
    // Just return a simple message for now
    const char* msg = "BMP format not supported in this version";
    return httpd_resp_send(req, msg, strlen(msg));
}

// Implementation of startCameraServer function
void startCameraServer()
{
    // Initialize network configuration
    initNetworkConfig();
    
    // Initialize NeoPixel
    initNeoPixel();
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 25;

    // Define URI handlers
    httpd_uri_t index_uri_def = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = index_handler,
        .user_ctx = NULL
#ifdef CONFIG_HTTPD_WS_SUPPORT
        ,
        .is_websocket = true,
        .handle_ws_control_frames = false,
        .supported_subprotocol = NULL
#endif
    };

    httpd_uri_t status_uri_def = {
        .uri = "/status",
        .method = HTTP_GET,
        .handler = status_handler,
        .user_ctx = NULL
#ifdef CONFIG_HTTPD_WS_SUPPORT
        ,
        .is_websocket = true,
        .handle_ws_control_frames = false,
        .supported_subprotocol = NULL
#endif
    };

    httpd_uri_t cmd_uri_def = {
        .uri = "/control",
        .method = HTTP_GET,
        .handler = cmd_handler,
        .user_ctx = NULL
#ifdef CONFIG_HTTPD_WS_SUPPORT
        ,
        .is_websocket = true,
        .handle_ws_control_frames = false,
        .supported_subprotocol = NULL
#endif
    };

    httpd_uri_t capture_uri_def = {
        .uri = "/capture",
        .method = HTTP_GET,
        .handler = capture_handler,
        .user_ctx = NULL
#ifdef CONFIG_HTTPD_WS_SUPPORT
        ,
        .is_websocket = true,
        .handle_ws_control_frames = false,
        .supported_subprotocol = NULL
#endif
    };

    httpd_uri_t stream_uri_def = {
        .uri = "/stream",
        .method = HTTP_GET,
        .handler = stream_handler,
        .user_ctx = NULL
#ifdef CONFIG_HTTPD_WS_SUPPORT
        ,
        .is_websocket = true,
        .handle_ws_control_frames = false,
        .supported_subprotocol = NULL
#endif
    };

    httpd_uri_t bmp_uri_def = {
        .uri = "/bmp",
        .method = HTTP_GET,
        .handler = bmp_handler,
        .user_ctx = NULL
#ifdef CONFIG_HTTPD_WS_SUPPORT
        ,
        .is_websocket = true,
        .handle_ws_control_frames = false,
        .supported_subprotocol = NULL
#endif
    };

    // GPIO control endpoints
    httpd_uri_t gpio_do_uri_def = {
        .uri = "/gpio/do",
        .method = HTTP_GET,
        .handler = gpio_do_handler,
        .user_ctx = NULL
#ifdef CONFIG_HTTPD_WS_SUPPORT
        ,
        .is_websocket = true,
        .handle_ws_control_frames = false,
        .supported_subprotocol = NULL
#endif
    };

    httpd_uri_t gpio_ai_read_uri_def = {
        .uri = "/gpio/ai/read",
        .method = HTTP_GET,
        .handler = gpio_ai_read_handler,
        .user_ctx = NULL
#ifdef CONFIG_HTTPD_WS_SUPPORT
        ,
        .is_websocket = true,
        .handle_ws_control_frames = false,
        .supported_subprotocol = NULL
#endif
    };

    httpd_uri_t gpio_ao_set_uri_def = {
        .uri = "/gpio/ao/set",
        .method = HTTP_GET,
        .handler = gpio_ao_set_handler,
        .user_ctx = NULL
#ifdef CONFIG_HTTPD_WS_SUPPORT
        ,
        .is_websocket = true,
        .handle_ws_control_frames = false,
        .supported_subprotocol = NULL
#endif
    };

    httpd_uri_t gpio_do_all_uri_def = {
        .uri = "/gpio/do/all",
        .method = HTTP_GET,
        .handler = gpio_do_all_handler,
        .user_ctx = NULL
#ifdef CONFIG_HTTPD_WS_SUPPORT
        ,
        .is_websocket = true,
        .handle_ws_control_frames = false,
        .supported_subprotocol = NULL
#endif
    };

    httpd_uri_t gpio_overview_uri_def = {
        .uri = "/gpio/overview",
        .method = HTTP_GET,
        .handler = gpio_overview_handler,
        .user_ctx = NULL
#ifdef CONFIG_HTTPD_WS_SUPPORT
        ,
        .is_websocket = true,
        .handle_ws_control_frames = false,
        .supported_subprotocol = NULL
#endif
    };

    // Network configuration endpoints
    httpd_uri_t network_config_get_uri_def = {
        .uri = "/network/config/get",
        .method = HTTP_GET,
        .handler = network_config_get_handler,
        .user_ctx = NULL
#ifdef CONFIG_HTTPD_WS_SUPPORT
        ,
        .is_websocket = true,
        .handle_ws_control_frames = false,
        .supported_subprotocol = NULL
#endif
    };

    httpd_uri_t network_config_set_uri_def = {
        .uri = "/network/config/set",
        .method = HTTP_GET,
        .handler = network_config_set_handler,
        .user_ctx = NULL
#ifdef CONFIG_HTTPD_WS_SUPPORT
        ,
        .is_websocket = true,
        .handle_ws_control_frames = false,
        .supported_subprotocol = NULL
#endif
    };

    httpd_uri_t restart_uri_def = {
        .uri = "/restart",
        .method = HTTP_GET,
        .handler = restart_handler,
        .user_ctx = NULL
#ifdef CONFIG_HTTPD_WS_SUPPORT
        ,
        .is_websocket = true,
        .handle_ws_control_frames = false,
        .supported_subprotocol = NULL
#endif
    };

    // NeoPixel control endpoints
    httpd_uri_t neopixel_set_uri_def = {
        .uri = "/neopixel/set",
        .method = HTTP_GET,
        .handler = neopixel_set_handler,
        .user_ctx = NULL
#ifdef CONFIG_HTTPD_WS_SUPPORT
        ,
        .is_websocket = true,
        .handle_ws_control_frames = false,
        .supported_subprotocol = NULL
#endif
    };

    httpd_uri_t neopixel_off_uri_def = {
        .uri = "/neopixel/off",
        .method = HTTP_GET,
        .handler = neopixel_off_handler,
        .user_ctx = NULL
#ifdef CONFIG_HTTPD_WS_SUPPORT
        ,
        .is_websocket = true,
        .handle_ws_control_frames = false,
        .supported_subprotocol = NULL
#endif
    };

    Serial.printf("Starting web server on port: '%d'\n", config.server_port);
    if (httpd_start(&camera_httpd, &config) == ESP_OK)
    {
        httpd_register_uri_handler(camera_httpd, &index_uri_def);
        httpd_register_uri_handler(camera_httpd, &cmd_uri_def);
        httpd_register_uri_handler(camera_httpd, &status_uri_def);
        httpd_register_uri_handler(camera_httpd, &capture_uri_def);
        httpd_register_uri_handler(camera_httpd, &bmp_uri_def);
        
        // Register GPIO control endpoints
        httpd_register_uri_handler(camera_httpd, &gpio_do_uri_def);
        httpd_register_uri_handler(camera_httpd, &gpio_ai_read_uri_def);
        httpd_register_uri_handler(camera_httpd, &gpio_ao_set_uri_def);
        httpd_register_uri_handler(camera_httpd, &gpio_do_all_uri_def);
        httpd_register_uri_handler(camera_httpd, &gpio_overview_uri_def);
        
        // Register network configuration endpoints
        httpd_register_uri_handler(camera_httpd, &network_config_get_uri_def);
        httpd_register_uri_handler(camera_httpd, &network_config_set_uri_def);
        httpd_register_uri_handler(camera_httpd, &restart_uri_def);
        
        // Register NeoPixel control endpoints
        httpd_register_uri_handler(camera_httpd, &neopixel_set_uri_def);
        httpd_register_uri_handler(camera_httpd, &neopixel_off_uri_def);
        
        // Register stream endpoint with the main server
        httpd_register_uri_handler(camera_httpd, &stream_uri_def);
    }

    Serial.println("Camera Server Started");
}
