// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// ==========================================================================
// This is the corrected version of app_httpd.cpp based on the user's code
// and the requirements to consolidate ports and fix compilation errors.
// - Single HTTP server on port 80.
// - Stream handler registered at /stream path.
// - Removed setup for the second (port 81) server.
// - index_handler modified to always serve the OV2640 HTML.
// - Spurious line with '®' removed.
// ==========================================================================

#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_camera.h"
#include "img_converters.h"
#include "fb_gfx.h"
#include "esp32-hal-ledc.h" // For LEDC control if used
#include "sdkconfig.h"
#include "camera_index.h" // Contains the HTML data arrays (index_ovXXXX_html_gz)
#include <Arduino.h>

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#endif
#include "utilities.h" // Board specific defines like IR_FILTER_NUM

// Face Detection configuration (remains as provided by user)
#ifdef BOARD_HAS_PSRAM
#define CONFIG_ESP_FACE_DETECT_ENABLED 0
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
#define TWO_STAGE 1
#if CONFIG_ESP_FACE_RECOGNITION_ENABLED
#include "face_recognition_tool.hpp"
#include "face_recognition_112_v1_s16.hpp"
#include "face_recognition_112_v1_s8.hpp"
#define QUANT_TYPE 0
#define FACE_ID_SAVE_NUMBER 7
#endif
#define FACE_COLOR_WHITE 0x00FFFFFF
#define FACE_COLOR_BLACK 0x00000000
#define FACE_COLOR_RED 0x000000FF
#define FACE_COLOR_GREEN 0x0000FF00
#define FACE_COLOR_BLUE 0x00FF0000
#define FACE_COLOR_YELLOW (FACE_COLOR_RED | FACE_COLOR_GREEN)
#define FACE_COLOR_CYAN (FACE_COLOR_BLUE | FACE_COLOR_GREEN)
#define FACE_COLOR_PURPLE (FACE_COLOR_BLUE | FACE_COLOR_RED)
#endif

// LED FLASH configuration (remains as provided)
#define CONFIG_LED_ILLUMINATOR_ENABLED 1
#if CONFIG_LED_ILLUMINATOR_ENABLED
// Note: LEDC setup might need adjustment based on camera channel usage
// #define LED_LEDC_CHANNEL 2 // Original example uses channel 2
// Use a different channel if camera uses 0/1
#define LED_LEDC_CHANNEL LEDC_CHANNEL_2
#define CONFIG_LED_MAX_INTENSITY 255
int led_duty = 0;
bool isStreaming = false;
#endif

// --- MJPEG Streaming Definitions ---
typedef struct {
    httpd_req_t *req;
    size_t len;
} jpg_chunking_t;

#define PART_BOUNDARY "123456789000000000000987654321"
static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\nX-Timestamp: %d.%06d\r\n\r\n";

// --- Server Handle ---
// Use only one handle for the main server on port 80
httpd_handle_t camera_httpd = NULL;
// REMOVED: httpd_handle_t stream_httpd = NULL;

// --- Face Detection / Recognition static variables (if enabled) ---
#if CONFIG_ESP_FACE_DETECT_ENABLED
static int8_t detection_enabled = 0;
// Instance creation might need adjustment based on resources/performance
// #if TWO_STAGE
// static HumanFaceDetectMSR01 s1(0.1F, 0.5F, 10, 0.2F);
// static HumanFaceDetectMNP01 s2(0.5F, 0.3F, 5);
// #else
// static HumanFaceDetectMSR01 s1(0.3F, 0.5F, 10, 0.2F);
// #endif
#if CONFIG_ESP_FACE_RECOGNITION_ENABLED
static int8_t recognition_enabled = 0;
static int8_t is_enrolling = 0;
#if QUANT_TYPE
FaceRecognition112V1S16 recognizer;
#else
FaceRecognition112V1S8 recognizer;
#endif
#endif // CONFIG_ESP_FACE_RECOGNITION_ENABLED
#endif // CONFIG_ESP_FACE_DETECT_ENABLED

// --- Rolling Average Filter ---
typedef struct {
    size_t size;
    size_t index;
    size_t count;
    int sum;
    int *values;
} ra_filter_t;

static ra_filter_t ra_filter;

static ra_filter_t *ra_filter_init(ra_filter_t *filter, size_t sample_size)
{
    memset(filter, 0, sizeof(ra_filter_t));

    filter->values = (int *)malloc(sample_size * sizeof(int));
    if (!filter->values) {
        return NULL;
    }
    memset(filter->values, 0, sample_size * sizeof(int));

    filter->size = sample_size;
    return filter;
}

// Only compile run function if logging is enabled to save space
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
static int ra_filter_run(ra_filter_t *filter, int value)
{
    if (!filter->values) {
        return value;
    }
    filter->sum -= filter->values[filter->index];
    filter->values[filter->index] = value;
    filter->sum += filter->values[filter->index];
    filter->index++;
    filter->index = filter->index % filter->size;
    if (filter->count < filter->size) {
        filter->count++;
    }
    // Avoid division by zero if count is somehow 0
    if (filter->count == 0) return 0;
    return filter->sum / filter->count;
}
#endif // ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO

// --- Face Detection Drawing / Recognition Helpers (Compile only if enabled) ---
#if CONFIG_ESP_FACE_DETECT_ENABLED
#if CONFIG_ESP_FACE_RECOGNITION_ENABLED
static void rgb_print(fb_data_t *fb, uint32_t color, const char *str)
{
    fb_gfx_print(fb, (fb->width - (strlen(str) * 14)) / 2, 10, color, str);
}

static int rgb_printf(fb_data_t *fb, uint32_t color, const char *format, ...)
{
    char loc_buf[64];
    char *temp = loc_buf;
    int len;
    va_list arg;
    va_list copy;
    va_start(arg, format);
    va_copy(copy, arg);
    len = vsnprintf(loc_buf, sizeof(loc_buf), format, arg);
    va_end(copy);
    if (len >= sizeof(loc_buf)) {
        temp = (char *)malloc(len + 1);
        if (temp == NULL) {
            return 0;
        }
    }
    vsnprintf(temp, len + 1, format, arg);
    va_end(arg);
    rgb_print(fb, color, temp);
    if (len > 64) { // Use >= here? vsnprintf returns size needed excluding null
        free(temp);
    }
    return len;
}
#endif // CONFIG_ESP_FACE_RECOGNITION_ENABLED

static void draw_face_boxes(fb_data_t *fb, std::list<dl::detect::result_t> *results, int face_id)
{
    int x, y, w, h;
    uint32_t color = FACE_COLOR_YELLOW;
    if (face_id < 0) {
        color = FACE_COLOR_RED;
    } else if (face_id > 0) {
        color = FACE_COLOR_GREEN;
    }
    // Adjust color format if using RGB565 framebuffer
    if (fb->bytes_per_pixel == 2 && fb->format == FB_RGB565) {
        color = ((color >> 16) & 0x001F) | ((color >> 3) & 0x07E0) | ((color << 8) & 0xF800);
    }
    int i = 0;
    for (std::list<dl::detect::result_t>::iterator prediction = results->begin(); prediction != results->end(); prediction++, i++) {
        // rectangle box
        x = (int)prediction->box[0];
        y = (int)prediction->box[1];
        w = (int)prediction->box[2] - x + 1;
        h = (int)prediction->box[3] - y + 1;
        // Clamp box to framebuffer dimensions
        if (x < 0) { w += x; x = 0;}
        if (y < 0) { h += y; y = 0;}
        if ((x + w) > fb->width) { w = fb->width - x; }
        if ((y + h) > fb->height) { h = fb->height - y; }

        if (w < 0) w = 0; // Prevent negative width/height
        if (h < 0) h = 0;

        fb_gfx_drawFastHLine(fb, x, y, w, color);
        fb_gfx_drawFastHLine(fb, x, y + h - 1, w, color);
        fb_gfx_drawFastVLine(fb, x, y, h, color);
        fb_gfx_drawFastVLine(fb, x + w - 1, y, h, color);
#if TWO_STAGE // Draw landmarks if using two-stage detector
        int x0, y0, j;
        for (j = 0; j < 10; j += 2) {
            x0 = (int)prediction->keypoint[j];
            y0 = (int)prediction->keypoint[j + 1];
            // Clamp landmarks too?
            if (x0 >= 0 && x0 < fb->width && y0 >= 0 && y0 < fb->height) {
                 fb_gfx_fillRect(fb, x0 -1 , y0 -1 , 3, 3, color); // Small square
            }
        }
#endif
    }
}

#if CONFIG_ESP_FACE_RECOGNITION_ENABLED
static int run_face_recognition(fb_data_t *fb, std::list<dl::detect::result_t> *results)
{
    if (results->empty()) return -2; // No face detected

    std::vector<int> landmarks = results->front().keypoint; // Use landmarks from the first detected face
    int id = -1;

    dl::Tensor<uint8_t> tensor;
    // Check framebuffer format before creating tensor
    if (fb->format != FB_BGR888 && fb->format != FB_RGB888) {
        log_e("Face recognition requires RGB888/BGR888 format!");
        // This function is usually called after conversion to RGB, so this might be redundant
        return -3; // Indicate format error
    }
    tensor.set_element((uint8_t *)fb->data).set_shape({(int)fb->height, (int)fb->width, 3}).set_auto_free(false);

    int enrolled_count = recognizer.get_enrolled_id_num();

    if (enrolled_count < FACE_ID_SAVE_NUMBER && is_enrolling) {
        id = recognizer.enroll_id(tensor, landmarks, "", true);
        log_i("Enrolled ID: %d", id);
        rgb_printf(fb, FACE_COLOR_CYAN, "ID[%u]", id);
    } else { // Only run recognition if not enrolling or storage is full
         if (is_enrolling) {
            rgb_printf(fb, FACE_COLOR_RED, "Storage Full");
            id = -4; // Indicate storage full
         } else {
            face_info_t recognize = recognizer.recognize(tensor, landmarks);
            if (recognize.id >= 0) {
                rgb_printf(fb, FACE_COLOR_GREEN, "ID[%u]: %.2f", recognize.id, recognize.similarity);
                id = recognize.id; // Return recognized ID
            } else {
                rgb_print(fb, FACE_COLOR_RED, "Intruder Alert!");
                id = -1; // Indicate intruder
            }
         }
    }
    return id;
}
#endif // CONFIG_ESP_FACE_RECOGNITION_ENABLED
#endif // CONFIG_ESP_FACE_DETECT_ENABLED

// --- LED Control Function ---
#if CONFIG_LED_ILLUMINATOR_ENABLED
void enable_led(bool en)
{
    // Simple ON/OFF based on 'isStreaming' and 'led_duty'
    // More sophisticated control (PWM) might be needed depending on hardware
    int duty = en ? led_duty : 0;
    // The LEDC functions might not be defined if not included/initialized properly
    // Assuming LEDC was setup correctly elsewhere (e.g., in setup() or a helper)
    // Check if channel is configured before writing
    if (ledcRead(LED_LEDC_CHANNEL) >= 0) { // A simple check if channel is valid/setup
         ledcWrite(LED_LEDC_CHANNEL, duty);
         //log_i("Set LED intensity to %d", duty);
    } else {
         // Fallback or log error if LEDC not setup
         // digitalWrite(LED_PIN, en ? HIGH: LOW); // Example fallback if using simple GPIO
         log_w("LEDC Channel %d not configured for LED.", LED_LEDC_CHANNEL);
    }
}
#endif // CONFIG_LED_ILLUMINATOR_ENABLED

// --- BMP Handler ---
static esp_err_t bmp_handler(httpd_req_t *req)
{
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
    uint64_t fr_start = esp_timer_get_time();
#endif
    fb = esp_camera_fb_get();
    if (!fb) {
        log_e("Camera capture failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/x-windows-bmp");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.bmp");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    char ts[32];
    snprintf(ts, 32, "%ld.%06ld", fb->timestamp.tv_sec, fb->timestamp.tv_usec);
    httpd_resp_set_hdr(req, "X-Timestamp", (const char *)ts);

    uint8_t *buf = NULL;
    size_t buf_len = 0;
    bool converted = frame2bmp(fb, &buf, &buf_len);
    esp_camera_fb_return(fb); // Return framebuffer AFTER use
    if (!converted) {
        log_e("BMP Conversion failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    res = httpd_resp_send(req, (const char *)buf, buf_len);
    free(buf); // Free the converted buffer
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
    uint64_t fr_end = esp_timer_get_time();
    log_i("BMP: %llums, %uB", (uint64_t)((fr_end - fr_start) / 1000), buf_len);
#endif
    return res;
}

// --- JPG Chunking Callback ---
static size_t jpg_encode_stream(void *arg, size_t index, const void *data, size_t len)
{
    jpg_chunking_t *j = (jpg_chunking_t *)arg;
    if (!index) {
        j->len = 0;
    }
    if (httpd_resp_send_chunk(j->req, (const char *)data, len) != ESP_OK) {
        return 0; // Signal error
    }
    j->len += len;
    return len; // Return bytes written
}

// --- Still Image Capture Handler ---
static esp_err_t capture_handler(httpd_req_t *req)
{
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
    int64_t fr_start = esp_timer_get_time();
#endif

#if CONFIG_LED_ILLUMINATOR_ENABLED
    enable_led(true);
    // Small delay might be needed depending on LED turn-on time
    vTaskDelay(pdMS_TO_TICKS(50)); // Adjust delay as needed (e.g., 50-150ms)
#endif

    fb = esp_camera_fb_get(); // Capture frame

#if CONFIG_LED_ILLUMINATOR_ENABLED
    // Turn LED off shortly after capture if needed, or leave it to stream handler
    // enable_led(false); // Optional: Turn off immediately
#endif

    if (!fb) {
        log_e("Camera capture failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    char ts[32];
    snprintf(ts, 32, "%ld.%06ld", fb->timestamp.tv_sec, fb->timestamp.tv_usec);
    httpd_resp_set_hdr(req, "X-Timestamp", (const char *)ts);

    size_t fb_len = 0; // Initialize length

#if CONFIG_ESP_FACE_DETECT_ENABLED
    // Face detection logic (only if enabled and frame size is suitable)
    if (detection_enabled && fb->width <= 400) { // Example check: only run detection on smaller frames
        #if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
            bool detected = false;
        #endif
        int face_id = 0;
        uint8_t * out_buf = NULL; // Buffer for RGB conversion if needed

        // Check if conversion to RGB888 is needed (common for detection)
        if (fb->format != PIXFORMAT_RGB888 && fb->format != PIXFORMAT_BGR888) {
            size_t out_len = fb->width * fb->height * 3;
            out_buf = (uint8_t *)malloc(out_len);
            if (!out_buf) {
                log_e("Face Detect: out_buf malloc failed");
                res = ESP_FAIL;
            } else {
                 bool converted = fmt2rgb888(fb->buf, fb->len, fb->format, out_buf);
                 if (!converted) {
                     log_e("Face Detect: Conversion to RGB888 failed");
                     free(out_buf);
                     out_buf = NULL;
                     res = ESP_FAIL;
                 }
            }
        } else {
            // Frame is already in a suitable format (maybe RGB565 if detector supports it?)
            // Or just use the buffer directly if it's BGR888/RGB888
             // For simplicity, let's assume we always convert or detection works on fb->buf directly
             // This part needs refinement based on the exact detector used
            log_w("Face detection on non-RGB888 format might need specific handling.");
             out_buf = fb->buf; // Risky if format mismatch
        }

        if (res == ESP_OK && out_buf != NULL) {
            fb_data_t rfb;
            rfb.width = fb->width;
            rfb.height = fb->height;
            rfb.data = out_buf; // Use the potentially converted buffer
            rfb.bytes_per_pixel = 3;
            rfb.format = FB_BGR888; // Assuming conversion target was BGR/RGB

            // --- Run Detection ---
            // This part depends heavily on the chosen detection library (s1, s2 instances)
            // The provided code has instance creation commented out, so this needs fixing
            // Placeholder for detection logic:
            std::list<dl::detect::result_t> results; // Assume this gets populated
            /* Example using the commented instances (needs uncommenting & proper init):
            #if TWO_STAGE
                HumanFaceDetectMSR01 s1(0.1F, 0.5F, 10, 0.2F); // Re-instantiate or use static? Be careful with state!
                HumanFaceDetectMNP01 s2(0.5F, 0.3F, 5);
                std::list<dl::detect::result_t> &candidates = s1.infer((uint8_t *)rfb.data, {(int)rfb.height, (int)rfb.width, 3});
                results = s2.infer((uint8_t *)rfb.data, {(int)rfb.height, (int)rfb.width, 3}, candidates);
            #else
                HumanFaceDetectMSR01 s1(0.3F, 0.5F, 10, 0.2F); // Re-instantiate or use static?
                results = s1.infer((uint8_t *)rfb.data, {(int)rfb.height, (int)rfb.width, 3});
            #endif
            */

            if (!results.empty()) {
                #if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
                    detected = true;
                #endif
                #if CONFIG_ESP_FACE_RECOGNITION_ENABLED
                    if (recognition_enabled) {
                        face_id = run_face_recognition(&rfb, &results);
                    }
                #endif
                draw_face_boxes(&rfb, &results, face_id);
            }
            // --- End Detection ---

            // Convert the (potentially modified) RGB buffer back to JPG for sending
            jpg_chunking_t jchunk = {req, 0};
            bool converted_to_jpg = fmt2jpg_cb(rfb.data, rfb.width * rfb.height * rfb.bytes_per_pixel, rfb.width, rfb.height, PIXFORMAT_RGB888, 90, jpg_encode_stream, &jchunk);
            if (!converted_to_jpg) {
                log_e("Face Detect: JPEG compression failed");
                res = ESP_FAIL;
            } else {
                 httpd_resp_send_chunk(req, NULL, 0); // Finalize chunked response
                 fb_len = jchunk.len; // Get length from chunking struct
            }
        }

        // Free conversion buffer if it was allocated
        if (out_buf != NULL && out_buf != fb->buf) {
            free(out_buf);
        }
        esp_camera_fb_return(fb); // Return original framebuffer
        fb = NULL; // Indicate fb is returned

        #if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
            int64_t fr_end = esp_timer_get_time();
            log_i("FACE: %uB %ums %s%d", (uint32_t)(fb_len), (uint32_t)((fr_end - fr_start) / 1000), detected ? "DETECTED " : "", face_id);
        #endif

    } else
#endif // CONFIG_ESP_FACE_DETECT_ENABLED
    {
        // --- Standard JPG Handling (No face detect or frame too large) ---
        if (fb->format == PIXFORMAT_JPEG) {
            fb_len = fb->len;
            res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
        } else {
            jpg_chunking_t jchunk = {req, 0};
            bool converted = frame2jpg_cb(fb, 80, jpg_encode_stream, &jchunk);
            if (!converted) {
                log_e("JPEG Conversion failed");
                res = ESP_FAIL;
            } else {
                 httpd_resp_send_chunk(req, NULL, 0); // Finalize chunked response
                 fb_len = jchunk.len;
            }
        }
        if (fb) { // Return framebuffer if it wasn't already returned in face detect block
             esp_camera_fb_return(fb);
             fb = NULL;
        }

        #if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
            int64_t fr_end = esp_timer_get_time();
            log_i("JPG: %uB %ums", (uint32_t)(fb_len), (uint32_t)((fr_end - fr_start) / 1000));
        #endif
    }

    // If there was an error during processing, send 500
    if (res != ESP_OK && httpd_resp_get_status(req)[0] == '2') { // Check if response already sent
         httpd_resp_send_500(req);
    }

    return res; // Return final status
}


// --- MJPEG Stream Handler ---
static esp_err_t stream_handler(httpd_req_t *req)
{
    camera_fb_t *fb = NULL;
    struct timeval _timestamp;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t *_jpg_buf = NULL;
    char *part_buf[128]; // Buffer for the multipart boundary header

    // Face detection/recognition variables if enabled
#if CONFIG_ESP_FACE_DETECT_ENABLED
    #if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
        int64_t fr_start = 0;
        // Add other timing variables if needed for detailed profiling
    #endif
    int face_id = 0;
    // --- Re-instantiate detectors here or make them static members/globals ---
    // --- Be mindful of memory usage and potential state issues if static ---
    // --- For simplicity, re-instantiating here (less efficient) ---
    #if TWO_STAGE
        HumanFaceDetectMSR01 s1(0.1F, 0.5F, 10, 0.2F);
        HumanFaceDetectMNP01 s2(0.5F, 0.3F, 5);
    #else
        HumanFaceDetectMSR01 s1(0.3F, 0.5F, 10, 0.2F);
    #endif
#endif // CONFIG_ESP_FACE_DETECT_ENABLED

    static int64_t last_frame = 0;
    if (!last_frame) {
        last_frame = esp_timer_get_time();
    }

    // Set streaming headers
    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK) {
        return res;
    }
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "X-Framerate", "60"); // Informative, not guaranteed

#if CONFIG_LED_ILLUMINATOR_ENABLED
    isStreaming = true;
    if (led_duty > 0) enable_led(true); // Turn on LED if intensity is set
#endif

    // --- Streaming Loop ---
    while (true) {
#if CONFIG_ESP_FACE_DETECT_ENABLED
    #if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
        bool detected = false; // Reset per frame
        fr_start = esp_timer_get_time(); // Reset timer per frame
    #endif
        face_id = 0; // Reset face ID per frame
#endif

        fb = esp_camera_fb_get();
        if (!fb) {
            log_e("Camera capture failed");
            res = ESP_FAIL;
        } else {
            _timestamp.tv_sec = fb->timestamp.tv_sec;
            _timestamp.tv_usec = fb->timestamp.tv_usec;

#if CONFIG_ESP_FACE_DETECT_ENABLED
            // --- Face Detection Logic within Stream ---
             if (detection_enabled && fb->width <= 400) { // Check if enabled and size is appropriate
                uint8_t *out_buf = NULL;
                bool processed = false;

                // Convert to RGB if necessary
                if (fb->format != PIXFORMAT_RGB888 && fb->format != PIXFORMAT_BGR888) {
                     size_t out_len = fb->width * fb->height * 3;
                     out_buf = (uint8_t *)malloc(out_len);
                     if (out_buf) {
                        if (fmt2rgb888(fb->buf, fb->len, fb->format, out_buf)) {
                           // Use out_buf for detection
                        } else {
                            log_e("Stream: RGB conversion failed");
                            free(out_buf);
                            out_buf = NULL; // Signal error
                        }
                    } else {
                        log_e("Stream: out_buf malloc failed");
                        // Continue without detection if malloc fails? Or break?
                    }
                } else {
                    out_buf = fb->buf; // Use framebuffer directly if already RGB
                }

                // If we have a buffer (original or converted)
                if (out_buf) {
                    fb_data_t rfb;
                    rfb.width = fb->width;
                    rfb.height = fb->height;
                    rfb.data = out_buf;
                    rfb.bytes_per_pixel = 3;
                    rfb.format = FB_BGR888; // Assume BGR for detection

                    std::list<dl::detect::result_t> results; // Needs population
                    #if TWO_STAGE
                        std::list<dl::detect::result_t> &candidates = s1.infer((uint8_t *)rfb.data, {(int)rfb.height, (int)rfb.width, 3});
                        results = s2.infer((uint8_t *)rfb.data, {(int)rfb.height, (int)rfb.width, 3}, candidates);
                    #else
                        results = s1.infer((uint8_t *)rfb.data, {(int)rfb.height, (int)rfb.width, 3});
                    #endif

                    if (!results.empty()) {
                        #if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
                            detected = true;
                        #endif
                        #if CONFIG_ESP_FACE_RECOGNITION_ENABLED
                            if (recognition_enabled || is_enrolling) { // Check both flags
                                face_id = run_face_recognition(&rfb, &results);
                            }
                        #endif
                        draw_face_boxes(&rfb, &results, face_id);
                    }

                    // Convert processed RGB buffer back to JPG
                    bool converted_to_jpg = fmt2jpg(rfb.data, rfb.width * rfb.height * rfb.bytes_per_pixel, rfb.width, rfb.height, PIXFORMAT_RGB888, 90, &_jpg_buf, &_jpg_buf_len);
                    if (!converted_to_jpg) {
                        log_e("Stream: fmt2jpg conversion failed");
                        res = ESP_FAIL;
                    }
                    processed = true; // Mark as processed
                }

                // Free conversion buffer if allocated
                if (out_buf != NULL && out_buf != fb->buf) {
                    free(out_buf);
                }
                 if (!processed) { // If processing failed or wasn't possible
                     // Fallback: Try to convert original frame if it wasn't JPEG
                     if (fb->format != PIXFORMAT_JPEG) {
                         if (!frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len)) {
                             log_e("Stream: Fallback frame2jpg failed");
                             res = ESP_FAIL;
                         }
                     } else { // Original was already JPEG
                         _jpg_buf = fb->buf;
                         _jpg_buf_len = fb->len;
                     }
                 }
                 esp_camera_fb_return(fb); // Return original fb
                 fb = NULL; // Mark as returned

            } else
#endif // CONFIG_ESP_FACE_DETECT_ENABLED
            {
                // --- Standard Stream Handling (No face detect or frame too large) ---
                if (fb->format != PIXFORMAT_JPEG) {
                    if (!frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len)) {
                        log_e("Stream: frame2jpg compression failed");
                        res = ESP_FAIL;
                    }
                    esp_camera_fb_return(fb); // Return non-jpeg buffer
                    fb = NULL;
                } else {
                    // Frame is already JPEG, use directly
                    _jpg_buf_len = fb->len;
                    _jpg_buf = fb->buf;
                    // Framebuffer (fb) will be returned later after sending
                }
            }
        } // End of fb processing block

        // Send the frame chunk if conversion/capture was successful
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }
        if (res == ESP_OK) {
            size_t hlen = snprintf((char *)part_buf, 128, _STREAM_PART, _jpg_buf_len, _timestamp.tv_sec, _timestamp.tv_usec);
            res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        }
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }

        // --- Frame Buffer Management ---
        if (fb) {
            // If fb still holds the JPEG buffer (wasn't converted)
            esp_camera_fb_return(fb);
            fb = NULL;
            _jpg_buf = NULL; // Buffer belonged to fb
        } else if (_jpg_buf) {
            // If _jpg_buf points to a buffer allocated by frame2jpg or fmt2jpg
            free(_jpg_buf);
            _jpg_buf = NULL;
        }
        // --- End Frame Buffer Management ---


        if (res != ESP_OK) {
            log_w("Send frame failed or loop aborted");
            break; // Exit loop on error
        }

        // Frame timing calculation and logging
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
        int64_t fr_end = esp_timer_get_time();
        int64_t frame_time = fr_end - last_frame;
        last_frame = fr_end; // Update last frame time
        frame_time /= 1000; // Convert to ms
        uint32_t avg_frame_time = ra_filter_run(&ra_filter, frame_time);

        // Construct log message conditionally based on face detect status
        char log_buffer[200]; // Increase buffer size
        int written = snprintf(log_buffer, sizeof(log_buffer),
                  "MJPG: %uB %ums (%.1ffps), AVG: %ums (%.1ffps)",
                  (uint32_t)(_jpg_buf_len),
                  (uint32_t)frame_time, 1000.0 / (uint32_t)(frame_time > 0 ? frame_time : 1), // Avoid div by zero
                  avg_frame_time, 1000.0 / (uint32_t)(avg_frame_time > 0 ? avg_frame_time: 1)  // Avoid div by zero
                 );

        #if CONFIG_ESP_FACE_DETECT_ENABLED
            // Append face detection info if it ran
            if (detection_enabled && fb->width <= 400) { // Check condition again
                // Add timing variables calculation here if needed (fr_ready, fr_face, etc.)
                snprintf(log_buffer + written, sizeof(log_buffer) - written,
                         //", DETECT_TIMING=%ums %s%d", // Example detailed timing
                         ", %s%d", // Simpler log
                         (detected) ? "DETECTED " : "", face_id
                        );
            }
        #endif
        log_i("%s", log_buffer); // Print the constructed log message
#else
         // Update last_frame even if not logging detailed info
         last_frame = esp_timer_get_time();
#endif // ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO

    } // End of while(true) loop

#if CONFIG_LED_ILLUMINATOR_ENABLED
    isStreaming = false;
    enable_led(false); // Turn off LED when stream stops
#endif

    return res; // Return final status
}


// --- Helper to Parse GET Request Parameters ---
static esp_err_t parse_get(httpd_req_t *req, char **obuf)
{
    char *buf = NULL;
    size_t buf_len = 0;

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = (char *)malloc(buf_len);
        if (!buf) {
            log_e("parse_get: Malloc failed");
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            *obuf = buf;
            return ESP_OK;
        }
        free(buf); // Free if query str extraction failed
    }
    // Send 404 if no query string or other error
    // httpd_resp_send_404(req); // Consider if 404 or 500 is more appropriate
    return ESP_FAIL; // Indicate failure
}


// --- Command Handler (/control) ---
static esp_err_t cmd_handler(httpd_req_t *req)
{
    char *buf = NULL;
    char variable[32] = {0}; // Initialize buffers
    char value[32] = {0};

    if (parse_get(req, &buf) != ESP_OK) {
        // parse_get sends 500 or 404 on failure
        return ESP_FAIL;
    }

    esp_err_t query_res1 = httpd_query_key_value(buf, "var", variable, sizeof(variable));
    esp_err_t query_res2 = httpd_query_key_value(buf, "val", value, sizeof(value));

    if (query_res1 != ESP_OK || query_res2 != ESP_OK) {
        free(buf);
        log_w("cmd_handler: Failed to get var/val from query string: %s", buf);
        httpd_resp_send_400(req); // Bad request
        return ESP_FAIL;
    }
    free(buf); // Free buffer after parsing

    // Sanitize or validate value? For now, atoi is used.
    int val = atoi(value);
    log_i("Set '%s' to '%s' (%d)", variable, value, val);

    sensor_t *s = esp_camera_sensor_get();
    if (!s) {
        log_e("Sensor not found!");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    int res = 0; // Result of sensor set function

    // --- Sensor Control Logic ---
    if (!strcmp(variable, "framesize")) {
        if (s->pixformat == PIXFORMAT_JPEG) {
            res = s->set_framesize(s, (framesize_t)val);
        } else {
             log_w("Can only change framesize in JPEG mode");
             res = -1; // Indicate error
        }
    } else if (!strcmp(variable, "quality")) res = s->set_quality(s, val);
    else if (!strcmp(variable, "contrast")) res = s->set_contrast(s, val);
    else if (!strcmp(variable, "brightness")) res = s->set_brightness(s, val);
    else if (!strcmp(variable, "saturation")) res = s->set_saturation(s, val);
    else if (!strcmp(variable, "gainceiling")) res = s->set_gainceiling(s, (gainceiling_t)val);
    else if (!strcmp(variable, "colorbar")) res = s->set_colorbar(s, val);
    else if (!strcmp(variable, "awb")) res = s->set_whitebal(s, val);
    else if (!strcmp(variable, "agc")) res = s->set_gain_ctrl(s, val);
    else if (!strcmp(variable, "aec")) res = s->set_exposure_ctrl(s, val);
    else if (!strcmp(variable, "hmirror")) res = s->set_hmirror(s, val);
    else if (!strcmp(variable, "vflip")) res = s->set_vflip(s, val);
    else if (!strcmp(variable, "awb_gain")) res = s->set_awb_gain(s, val);
    else if (!strcmp(variable, "agc_gain")) res = s->set_agc_gain(s, val);
    else if (!strcmp(variable, "aec_value")) res = s->set_aec_value(s, val);
    else if (!strcmp(variable, "aec2")) res = s->set_aec2(s, val);
    else if (!strcmp(variable, "dcw")) res = s->set_dcw(s, val);
    else if (!strcmp(variable, "bpc")) res = s->set_bpc(s, val);
    else if (!strcmp(variable, "wpc")) res = s->set_wpc(s, val);
    else if (!strcmp(variable, "raw_gma")) res = s->set_raw_gma(s, val);
    else if (!strcmp(variable, "lenc")) res = s->set_lenc(s, val);
    else if (!strcmp(variable, "special_effect")) res = s->set_special_effect(s, val);
    else if (!strcmp(variable, "wb_mode")) res = s->set_wb_mode(s, val);
    else if (!strcmp(variable, "ae_level")) res = s->set_ae_level(s, val);
#if CONFIG_LED_ILLUMINATOR_ENABLED
    else if (!strcmp(variable, "led_intensity")) {
        led_duty = constrain(val, 0, CONFIG_LED_MAX_INTENSITY); // Constrain value
        if (isStreaming || led_duty > 0) // Enable only if streaming or intensity > 0? Adjust logic.
            enable_led(true); // Apply immediately if needed
        else
             enable_led(false); // Turn off if intensity is 0 and not streaming
        res = 0; // Assume success for LED control
    }
#endif // CONFIG_LED_ILLUMINATOR_ENABLED
    else if (!strcmp(variable, "ir")) {
        #ifdef IR_FILTER_NUM
          if (IR_FILTER_NUM >= 0) {
              digitalWrite(IR_FILTER_NUM, val ? HIGH : LOW); // Control IR filter
              res = 0; // Assume success
          } else {
               log_w("IR filter pin not defined");
               res = -1;
          }
        #else
            log_w("IR filter support not compiled");
            res = -1;
        #endif
    }
#if CONFIG_ESP_FACE_DETECT_ENABLED
    else if (!strcmp(variable, "face_detect")) {
        if (val && s->status.framesize > FRAMESIZE_CIF) { // Example check
            log_w("Face Detect requires CIF or lower resolution");
            res = -1; // Prevent enabling if resolution too high
        } else {
             detection_enabled = val;
             #if CONFIG_ESP_FACE_RECOGNITION_ENABLED
             if (!detection_enabled) {
                 recognition_enabled = 0; // Disable recognition if detection is off
                 is_enrolling = 0; // Disable enrolling too
             }
             #endif
             res = 0;
        }
    }
#if CONFIG_ESP_FACE_RECOGNITION_ENABLED
    else if (!strcmp(variable, "face_enroll")) {
        if (detection_enabled) { // Can only enroll if detection is on
            is_enrolling = !is_enrolling; // Toggle enrollment state
            log_i("Enrolling: %s", is_enrolling ? "true" : "false");
            res = 0;
        } else {
            log_w("Enable Face Detection before enrolling.");
            res = -1;
        }
    } else if (!strcmp(variable, "face_recognize")) {
         if (val && !detection_enabled) {
             log_w("Face Recognition requires Face Detection to be enabled.");
             res = -1;
         } else {
             recognition_enabled = val;
             res = 0;
         }
    }
#endif // CONFIG_ESP_FACE_RECOGNITION_ENABLED
#endif // CONFIG_ESP_FACE_DETECT_ENABLED
    else {
        log_w("Unknown command: %s", variable);
        res = -1; // Indicate unknown command
    }

    if (res < 0) {
        log_e("Control command '%s' failed or was invalid.", variable);
        // Send 500 for sensor errors, maybe 400 for invalid commands?
        return httpd_resp_send_500(req);
    }

    // Send OK response
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, NULL, 0);
}

// --- Helper to Print Sensor Register Status (Used in status_handler) ---
static int print_reg(char *p, sensor_t *s, uint16_t reg, uint32_t mask)
{
    // Ensure mask is not zero to avoid issues with get_reg if it doesn't handle it
    if (mask == 0) mask = 0xFF;
    // Ensure p is not NULL
    if (!p || !s) return 0;
    int val = s->get_reg(s, reg, mask);
    // Check for error from get_reg? Original code didn't.
    return sprintf(p, "\"0x%x\":%u,", reg, (val < 0)? 0 : (unsigned int)val); // Print 0 on error?
}

// --- Status Handler (/status) ---
static esp_err_t status_handler(httpd_req_t *req)
{
    // Increased buffer size for potentially more registers/settings
    static char json_response[2048];

    sensor_t *s = esp_camera_sensor_get();
    if (!s) {
        log_e("status_handler: Sensor not found!");
        return httpd_resp_send_500(req);
    }

    char *p = json_response;
    *p++ = '{';

    // --- Add Register Values (Sensor Specific) ---
    // Consider adding checks if s->get_reg is valid before calling
    if (s->id.PID == OV5640_PID || s->id.PID == OV3660_PID) {
         // Include relevant registers for these sensors if known
         // Example (from original):
         for (int reg = 0x3400; reg < 0x3406; reg += 2) {
             p += print_reg(p, s, reg, 0xFFF);
         }
         p += print_reg(p, s, 0x3406, 0xFF);
         p += print_reg(p, s, 0x3500, 0xFFFF0); // Check mask validity
         p += print_reg(p, s, 0x3503, 0xFF);
         p += print_reg(p, s, 0x350a, 0x3FF);
         p += print_reg(p, s, 0x350c, 0xFFFF);
         // Add more registers as needed...
    } else if (s->id.PID == OV2640_PID) {
        p += print_reg(p, s, 0xd3, 0xFF);
        p += print_reg(p, s, 0x111, 0xFF);
        p += print_reg(p, s, 0x132, 0xFF);
    }
    // Add sections for other supported PIDs if necessary

    // --- Add Standard Camera Status Values ---
    // Use snprintf for safety against buffer overflows
    p += snprintf(p, sizeof(json_response) - (p - json_response), "\"xclk\":%u,", s->xclk_freq_hz / 1000000);
    p += snprintf(p, sizeof(json_response) - (p - json_response), "\"pixformat\":%u,", s->pixformat);
    p += snprintf(p, sizeof(json_response) - (p - json_response), "\"framesize\":%u,", s->status.framesize);
    p += snprintf(p, sizeof(json_response) - (p - json_response), "\"quality\":%u,", s->status.quality);
    p += snprintf(p, sizeof(json_response) - (p - json_response), "\"brightness\":%d,", s->status.brightness);
    p += snprintf(p, sizeof(json_response) - (p - json_response), "\"contrast\":%d,", s->status.contrast);
    p += snprintf(p, sizeof(json_response) - (p - json_response), "\"saturation\":%d,", s->status.saturation);
    p += snprintf(p, sizeof(json_response) - (p - json_response), "\"sharpness\":%d,", s->status.sharpness); // Sharpness wasn't in original? Add if needed.
    p += snprintf(p, sizeof(json_response) - (p - json_response), "\"special_effect\":%u,", s->status.special_effect);
    p += snprintf(p, sizeof(json_response) - (p - json_response), "\"wb_mode\":%u,", s->status.wb_mode);
    p += snprintf(p, sizeof(json_response) - (p - json_response), "\"awb\":%u,", s->status.awb);
    p += snprintf(p, sizeof(json_response) - (p - json_response), "\"awb_gain\":%u,", s->status.awb_gain);
    p += snprintf(p, sizeof(json_response) - (p - json_response), "\"aec\":%u,", s->status.aec);
    p += snprintf(p, sizeof(json_response) - (p - json_response), "\"aec2\":%u,", s->status.aec2);
    p += snprintf(p, sizeof(json_response) - (p - json_response), "\"ae_level\":%d,", s->status.ae_level);
    p += snprintf(p, sizeof(json_response) - (p - json_response), "\"aec_value\":%u,", s->status.aec_value);
    p += snprintf(p, sizeof(json_response) - (p - json_response), "\"agc\":%u,", s->status.agc);
    p += snprintf(p, sizeof(json_response) - (p - json_response), "\"agc_gain\":%u,", s->status.agc_gain);
    p += snprintf(p, sizeof(json_response) - (p - json_response), "\"gainceiling\":%u,", s->status.gainceiling);
    p += snprintf(p, sizeof(json_response) - (p - json_response), "\"bpc\":%u,", s->status.bpc);
    p += snprintf(p, sizeof(json_response) - (p - json_response), "\"wpc\":%u,", s->status.wpc);
    p += snprintf(p, sizeof(json_response) - (p - json_response), "\"raw_gma\":%u,", s->status.raw_gma);
    p += snprintf(p, sizeof(json_response) - (p - json_response), "\"lenc\":%u,", s->status.lenc);
    p += snprintf(p, sizeof(json_response) - (p - json_response), "\"hmirror\":%u,", s->status.hmirror);
    p += snprintf(p, sizeof(json_response) - (p - json_response), "\"vflip\":%u,", s->status.vflip); // Added vflip
    p += snprintf(p, sizeof(json_response) - (p - json_response), "\"dcw\":%u,", s->status.dcw);
    p += snprintf(p, sizeof(json_response) - (p - json_response), "\"colorbar\":%u", s->status.colorbar); // Last standard item, no trailing comma

    // Add LED intensity status if enabled
#if CONFIG_LED_ILLUMINATOR_ENABLED
    p += snprintf(p, sizeof(json_response) - (p - json_response), ",\"led_intensity\":%u", led_duty);
#else
    p += snprintf(p, sizeof(json_response) - (p - json_response), ",\"led_intensity\":%d", -1); // Indicate disabled
#endif

    // Add IR Filter status if enabled
#ifdef IR_FILTER_NUM
      if (IR_FILTER_NUM >= 0) {
         p += snprintf(p, sizeof(json_response) - (p - json_response), ",\"ir\":%u", digitalRead(IR_FILTER_NUM));
      } else {
         p += snprintf(p, sizeof(json_response) - (p - json_response), ",\"ir\":%d", -1); // Indicate disabled/not defined
      }
#endif


    // Add face detection status if enabled
#if CONFIG_ESP_FACE_DETECT_ENABLED
    p += snprintf(p, sizeof(json_response) - (p - json_response), ",\"face_detect\":%u", detection_enabled);
#if CONFIG_ESP_FACE_RECOGNITION_ENABLED
    p += snprintf(p, sizeof(json_response) - (p - json_response), ",\"face_enroll\":%u", is_enrolling);
    p += snprintf(p, sizeof(json_response) - (p - json_response), ",\"face_recognize\":%u", recognition_enabled);
#endif // CONFIG_ESP_FACE_RECOGNITION_ENABLED
#endif // CONFIG_ESP_FACE_DETECT_ENABLED

    // Check if buffer might be full before closing brace
    if ((p - json_response) < (sizeof(json_response) - 2)) {
        *p++ = '}';
        *p++ = 0; // Null-terminate
    } else {
        // Buffer is full or very close, overwrite last chars to ensure valid JSON
        json_response[sizeof(json_response) - 2] = '}';
        json_response[sizeof(json_response) - 1] = 0;
        log_w("Status JSON response buffer truncated!");
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json_response, strlen(json_response));
}

// --- XCLK Handler (/xclk) ---
static esp_err_t xclk_handler(httpd_req_t *req)
{
    char *buf = NULL;
    char _xclk[32];

    if (parse_get(req, &buf) != ESP_OK) {
        return ESP_FAIL; // Response already sent by parse_get
    }
    if (httpd_query_key_value(buf, "xclk", _xclk, sizeof(_xclk)) != ESP_OK) {
        free(buf);
        log_w("xclk_handler: Missing 'xclk' query parameter.");
        httpd_resp_send_400(req); // Bad request
        return ESP_FAIL;
    }
    free(buf);

    int xclk = atoi(_xclk);
    log_i("Set XCLK: %d MHz", xclk);

    sensor_t *s = esp_camera_sensor_get();
    if (!s) {
        log_e("Sensor not found!");
        return httpd_resp_send_500(req);
    }
    // TODO: Check if set_xclk function exists in sensor struct?
    int res = s->set_xclk(s, LEDC_TIMER_0, xclk);
    if (res != ESP_OK) { // Check return value properly
        log_e("set_xclk failed with error %d", res);
        return httpd_resp_send_500(req);
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, NULL, 0);
}


// --- Register Set Handler (/reg) ---
static esp_err_t reg_handler(httpd_req_t *req)
{
    char *buf = NULL;
    char _reg[32], _mask[32], _val[32]; // Buffers for parameters

    if (parse_get(req, &buf) != ESP_OK) {
        return ESP_FAIL;
    }

    esp_err_t res1 = httpd_query_key_value(buf, "reg", _reg, sizeof(_reg));
    esp_err_t res2 = httpd_query_key_value(buf, "mask", _mask, sizeof(_mask));
    esp_err_t res3 = httpd_query_key_value(buf, "val", _val, sizeof(_val));

    if (res1 != ESP_OK || res2 != ESP_OK || res3 != ESP_OK) {
        free(buf);
        log_w("reg_handler: Missing reg/mask/val query parameters.");
        httpd_resp_send_400(req); // Bad request
        return ESP_FAIL;
    }
    free(buf);

    // Parse values (consider using strtol for better hex handling)
    int reg = strtol(_reg, NULL, 0); // Base 0 auto-detects 0x prefix
    int mask = strtol(_mask, NULL, 0);
    int val = strtol(_val, NULL, 0);
    log_i("Set Register: reg: 0x%02x, mask: 0x%02x, value: 0x%02x", reg, mask, val);

    sensor_t *s = esp_camera_sensor_get();
     if (!s) {
        log_e("Sensor not found!");
        return httpd_resp_send_500(req);
    }

    int res = s->set_reg(s, reg, mask, val);
    if (res != ESP_OK) { // Check return value
         log_e("set_reg failed with error %d", res);
        return httpd_resp_send_500(req);
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, NULL, 0);
}

// --- Register Get Handler (/greg) ---
static esp_err_t greg_handler(httpd_req_t *req)
{
    char *buf = NULL;
    char _reg[32], _mask[32];

    if (parse_get(req, &buf) != ESP_OK) {
        return ESP_FAIL;
    }

    esp_err_t res1 = httpd_query_key_value(buf, "reg", _reg, sizeof(_reg));
    esp_err_t res2 = httpd_query_key_value(buf, "mask", _mask, sizeof(_mask));

    if (res1 != ESP_OK || res2 != ESP_OK) {
        free(buf);
        log_w("greg_handler: Missing reg/mask query parameters.");
        httpd_resp_send_400(req); // Bad request
        return ESP_FAIL;
    }
    free(buf);

    int reg = strtol(_reg, NULL, 0);
    int mask = strtol(_mask, NULL, 0);

    sensor_t *s = esp_camera_sensor_get();
    if (!s) {
        log_e("Sensor not found!");
        return httpd_resp_send_500(req);
    }

    int res_val = s->get_reg(s, reg, mask);
    if (res_val < 0) { // get_reg often returns negative on error
        log_e("get_reg failed with error %d", res_val);
        return httpd_resp_send_500(req);
    }
    log_i("Get Register: reg: 0x%02x, mask: 0x%02x, value: 0x%02x", reg, mask, res_val);

    char buffer[20];
    snprintf(buffer, sizeof(buffer), "%d", res_val); // Send decimal value back

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, buffer, strlen(buffer));
}

// --- Helper to Parse Integer GET Variable ---
static int parse_get_var(char *buf, const char *key, int def)
{
    char _int[16];
    if (httpd_query_key_value(buf, key, _int, sizeof(_int)) != ESP_OK) {
        return def;
    }
    return atoi(_int); // Consider using strtol for robustness
}

// --- PLL Handler (/pll) ---
static esp_err_t pll_handler(httpd_req_t *req)
{
    char *buf = NULL;
    if (parse_get(req, &buf) != ESP_OK) {
        return ESP_FAIL;
    }

    int bypass = parse_get_var(buf, "bypass", 0);
    int mul = parse_get_var(buf, "mul", 0);
    int sys = parse_get_var(buf, "sys", 0);
    int root = parse_get_var(buf, "root", 0);
    int pre = parse_get_var(buf, "pre", 0);
    int seld5 = parse_get_var(buf, "seld5", 0);
    int pclken = parse_get_var(buf, "pclken", 0);
    int pclk = parse_get_var(buf, "pclk", 0);
    free(buf);

    log_i("Set Pll: bypass: %d, mul: %d, sys: %d, root: %d, pre: %d, seld5: %d, pclken: %d, pclk: %d", bypass, mul, sys, root, pre, seld5, pclken, pclk);
    sensor_t *s = esp_camera_sensor_get();
     if (!s) {
        log_e("Sensor not found!");
        return httpd_resp_send_500(req);
    }

    // Check if set_pll function pointer exists?
    if (!s->set_pll) {
         log_e("Sensor does not support set_pll");
         return httpd_resp_send_501(req); // Not Implemented
    }

    int res = s->set_pll(s, bypass, mul, sys, root, pre, seld5, pclken, pclk);
    if (res != ESP_OK) {
        log_e("set_pll failed with error %d", res);
        return httpd_resp_send_500(req);
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, NULL, 0);
}

// --- Window Handler (/resolution) ---
static esp_err_t win_handler(httpd_req_t *req)
{
    char *buf = NULL;
    if (parse_get(req, &buf) != ESP_OK) {
        return ESP_FAIL;
    }

    // Parse all window parameters
    int startX = parse_get_var(buf, "sx", 0);
    int startY = parse_get_var(buf, "sy", 0);
    int endX = parse_get_var(buf, "ex", 0);
    int endY = parse_get_var(buf, "ey", 0);
    int offsetX = parse_get_var(buf, "offx", 0);
    int offsetY = parse_get_var(buf, "offy", 0);
    int totalX = parse_get_var(buf, "tx", 0);
    int totalY = parse_get_var(buf, "ty", 0);
    int outputX = parse_get_var(buf, "ox", 0);
    int outputY = parse_get_var(buf, "oy", 0);
    bool scale = parse_get_var(buf, "scale", 0) == 1;
    bool binning = parse_get_var(buf, "binning", 0) == 1;
    free(buf);

    log_i("Set Window: Start: %d %d, End: %d %d, Offset: %d %d, Total: %d %d, Output: %d %d, Scale: %u, Binning: %u", startX, startY, endX, endY, offsetX, offsetY, totalX, totalY, outputX, outputY, scale, binning);

    sensor_t *s = esp_camera_sensor_get();
     if (!s) {
        log_e("Sensor not found!");
        return httpd_resp_send_500(req);
    }

    // Check if set_res_raw function pointer exists?
     if (!s->set_res_raw) {
         log_e("Sensor does not support set_res_raw");
         return httpd_resp_send_501(req); // Not Implemented
    }

    int res = s->set_res_raw(s, startX, startY, endX, endY, offsetX, offsetY, totalX, totalY, outputX, outputY, scale, binning);
    if (res != ESP_OK) {
        log_e("set_res_raw failed with error %d", res);
        return httpd_resp_send_500(req);
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, NULL, 0);
}


// --- Index Handler (Root /) ---
// Corrected version that always sends OV2640 HTML
static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    sensor_t *s = esp_camera_sensor_get();
    if (s != NULL) {
        // --- Always return the OV2640 HTML since others are not defined ---
        log_i("Sensor PID: 0x%x, sending OV2640 HTML.", s->id.PID); // Log the actual sensor
        // Ensure index_ov2640_html_gz and its length are defined in camera_index.h
        return httpd_resp_send(req, (const char *)index_ov2640_html_gz, index_ov2640_html_gz_len);
    } else {
        log_e("Camera sensor not found");
        // Optional: could still try to send the default page, but 500 is cleaner
        return httpd_resp_send_500(req);
    }
}


// --- Function to Start the Web Server ---
void startCameraServer()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16; // Adjusted handler count
    config.server_port = 80;      // Set port to 80
    config.ctrl_port = 32768;     // Default control port

    // --- URI Handler Definitions ---
    // (Ensure these structs are defined correctly as before)
    httpd_uri_t index_uri = {
        .uri = "/", .method = HTTP_GET, .handler = index_handler, .user_ctx = NULL
        #ifdef CONFIG_HTTPD_WS_SUPPORT
        , .is_websocket = false // Adjust if using websockets
        #endif
    };
    httpd_uri_t status_uri = {
        .uri = "/status", .method = HTTP_GET, .handler = status_handler, .user_ctx = NULL
        #ifdef CONFIG_HTTPD_WS_SUPPORT
        , .is_websocket = false
        #endif
    };
     httpd_uri_t cmd_uri = {
        .uri = "/control", .method = HTTP_GET, .handler = cmd_handler, .user_ctx = NULL
        #ifdef CONFIG_HTTPD_WS_SUPPORT
        , .is_websocket = false
        #endif
    };
    httpd_uri_t capture_uri = {
        .uri = "/capture", .method = HTTP_GET, .handler = capture_handler, .user_ctx = NULL
        #ifdef CONFIG_HTTPD_WS_SUPPORT
        , .is_websocket = false
        #endif
    };
    httpd_uri_t stream_uri = { // Define the stream handler URI
        .uri = "/stream", .method = HTTP_GET, .handler = stream_handler, .user_ctx = NULL
        #ifdef CONFIG_HTTPD_WS_SUPPORT
        , .is_websocket = false
        #endif
    };
     httpd_uri_t bmp_uri = {
        .uri = "/bmp", .method = HTTP_GET, .handler = bmp_handler, .user_ctx = NULL
        #ifdef CONFIG_HTTPD_WS_SUPPORT
        , .is_websocket = false
        #endif
    };
    httpd_uri_t xclk_uri = {
        .uri = "/xclk", .method = HTTP_GET, .handler = xclk_handler, .user_ctx = NULL
        #ifdef CONFIG_HTTPD_WS_SUPPORT
        , .is_websocket = false
        #endif
    };
     httpd_uri_t reg_uri = {
        .uri = "/reg", .method = HTTP_GET, .handler = reg_handler, .user_ctx = NULL
        #ifdef CONFIG_HTTPD_WS_SUPPORT
        , .is_websocket = false
        #endif
    };
     httpd_uri_t greg_uri = {
        .uri = "/greg", .method = HTTP_GET, .handler = greg_handler, .user_ctx = NULL
        #ifdef CONFIG_HTTPD_WS_SUPPORT
        , .is_websocket = false
        #endif
    };
     httpd_uri_t pll_uri = {
        .uri = "/pll", .method = HTTP_GET, .handler = pll_handler, .user_ctx = NULL
        #ifdef CONFIG_HTTPD_WS_SUPPORT
        , .is_websocket = false
        #endif
    };
     httpd_uri_t win_uri = {
        .uri = "/resolution", .method = HTTP_GET, .handler = win_handler, .user_ctx = NULL
        #ifdef CONFIG_HTTPD_WS_SUPPORT
        , .is_websocket = false
        #endif
    };

    // Initialize rolling average filter
    ra_filter_init(&ra_filter, 20);

#if CONFIG_ESP_FACE_RECOGNITION_ENABLED
    // Initialize face recognition if enabled
    recognizer.set_partition(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "fr");
    recognizer.set_ids_from_flash();
#endif

    // Start the single HTTP server
    Serial.printf("Starting web server on port: '%d'\n", config.server_port);
    if (httpd_start(&camera_httpd, &config) == ESP_OK) {
        // Register all URI handlers for the single server
        httpd_register_uri_handler(camera_httpd, &index_uri);
        httpd_register_uri_handler(camera_httpd, &cmd_uri);
        httpd_register_uri_handler(camera_httpd, &status_uri);
        httpd_register_uri_handler(camera_httpd, &capture_uri);
        httpd_register_uri_handler(camera_httpd, &bmp_uri);
        httpd_register_uri_handler(camera_httpd, &stream_uri); // Register the stream handler

        // Register other handlers
        httpd_register_uri_handler(camera_httpd, &xclk_uri);
        httpd_register_uri_handler(camera_httpd, ®_uri);
        httpd_register_uri_handler(camera_httpd, &greg_uri);
        httpd_register_uri_handler(camera_httpd, &pll_uri);
        httpd_register_uri_handler(camera_httpd, &win_uri);

        log_i("Web server started successfully.");

    } else {
        log_e("Error starting web server!");
    }

    // No second server start needed
}


// --- Function to Setup LED Flash (if used) ---
void setupLedFlash(int pin) // Note: Pin parameter might not be used if LEDC channel is hardcoded
{
#if CONFIG_LED_ILLUMINATOR_ENABLED
    // Configure LEDC channel for LED control
    // Frequency (5000 Hz) and resolution (8 bit -> 0-255)
    ledcSetup(LED_LEDC_CHANNEL, 5000, 8);
    // Attach the LED pin to the configured LEDC channel
    ledcAttachPin(pin, LED_LEDC_CHANNEL);
    log_i("LED flash configured on pin %d, LEDC channel %d", pin, LED_LEDC_CHANNEL);
#else
    log_i("LED flash is disabled via CONFIG_LED_ILLUMINATOR_ENABLED = 0");
#endif
}