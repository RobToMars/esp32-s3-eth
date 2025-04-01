#ifndef APP_HTTPD_WRAPPER_H
#define APP_HTTPD_WRAPPER_H

// Include guards for the original files
#ifndef APP_HTTPD_INCLUDED
#define APP_HTTPD_INCLUDED

// Forward declarations for functions in app_httpd.cpp
bool is_pin_safe(int pin);
bool is_pin_safe_ai(int pin);
bool is_pin_safe_ao(int pin);
int initDigitalOutput(int pin);
int setDigitalOutput(int pin, int value);
int readAnalogInput(int pin);
int setAnalogOutput(int pin, int value);
void startCameraServer();

#endif // APP_HTTPD_INCLUDED

#endif // APP_HTTPD_WRAPPER_H
