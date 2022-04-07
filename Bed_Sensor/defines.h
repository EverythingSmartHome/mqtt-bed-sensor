/****************************************************************************************************************************
  defines.h
  For any WiFi shields, such as WiFiNINA W101, W102, W13x, or custom, such as ESP8266/ESP32-AT, Ethernet, etc
  
  WiFiWebServer is a library for the ESP32-based WiFi shields to run WebServer
  Based on and modified from ESP8266 https://github.com/esp8266/Arduino/releases
  Based on  and modified from Arduino WiFiNINA library https://www.arduino.cc/en/Reference/WiFiNINA
  Built by Khoi Hoang https://github.com/khoih-prog/WiFiWebServer
  Licensed under MIT license
 ***************************************************************************************************************************************/

#ifndef defines_h
#define defines_h

#define DEBUG_WIFI_WEBSERVER_PORT   Serial

// Debug Level from 0 to 4
#define _WIFI_LOGLEVEL_             1
#define _WIFININA_LOGLEVEL_         3

#if (ESP32)

  #define USE_WIFI_NINA         false

  // To use the default WiFi library here 
  #define USE_WIFI_CUSTOM       false

#elif (ESP8266)

  #define USE_WIFI_NINA         false

  // To use the default WiFi library here 
  #define USE_WIFI_CUSTOM       true

#else

  #define USE_WIFI_NINA         false
  #define USE_WIFI101           false
  
  // If not USE_WIFI_NINA, you can USE_WIFI_CUSTOM, then include the custom WiFi library here 
  #define USE_WIFI_CUSTOM       true

#endif

#if (ESP8266)
  #warning Including ESP8266WiFi.h
  #include "ESP8266WiFi.h"
#endif

#if (ESP32 || ESP8266)
  #warning Using ESP WiFi with WiFi Library
  #define SHIELD_TYPE           "ESP WiFi using WiFi Library"  
#elif USE_WIFI_CUSTOM
  #warning Using Custom WiFi using Custom WiFi Library
  #define SHIELD_TYPE           "Custom WiFi using Custom WiFi Library"
#else
  #define SHIELD_TYPE           "Unknown WiFi shield/Library" 
#endif

#if defined(ESP32)
  #warning ESP32 board selected
  #define BOARD_TYPE  "ESP32"
#elif defined(ESP8266)
  #warning ESP8266 board selected
  #define BOARD_TYPE  "ESP8266"
#else
  #define BOARD_TYPE      "AVR Mega"
#endif

#ifndef BOARD_NAME
  #if defined(ARDUINO_BOARD)
    #define BOARD_NAME    ARDUINO_BOARD
  #elif defined(BOARD_TYPE)
    #define BOARD_NAME    BOARD_TYPE
  #else
    #define BOARD_NAME    "Unknown Board"
  #endif  
#endif

#warning Including WiFiWebServer.h
#include <WiFiWebServer.h>

#endif    //defines_h
