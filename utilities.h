
#pragma once

#define WAVESHARE_ESP32_S3_ETH

#if defined(WAVESHARE_ESP32_S3_ETH)
#define ETH_MISO_PIN                    12
#define ETH_MOSI_PIN                    11
#define ETH_SCLK_PIN                    13
#define ETH_CS_PIN                      14
#define ETH_INT_PIN                     10
#define ETH_RST_PIN                     9
#define ETH_ADDR                        1


#define IR_FILTER_NUM                   -1

#else
#error "Use ArduinoIDE, please open the macro definition corresponding to the board above <utilities.h>"
#endif









