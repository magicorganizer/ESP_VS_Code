#pragma once

#include <Arduino.h>
#include <Adafruit_ST7789.h>
#include <WiFi.h>

// Ensure LCD size macros are available if not already defined
#ifndef LCD_WIDTH
#define LCD_WIDTH 170
#endif
#ifndef LCD_HEIGHT
#define LCD_HEIGHT 320
#endif

// Extern globals from main.cpp
extern Adafruit_ST7789 lcd;
extern const uint16_t imageData[];
extern const char* ssid;
extern const char* password;
extern WiFiServer server;
extern String header;
extern String output26State;
extern String output27State;
extern const int output26;
extern const int output27;

// Timer handler used by App task
extern void handle_timers();

// Task prototypes
void App_Main(void *pvParameters);
void WebInterface_Main(void *pvParameters);
void Display_Main(void *pvParameters);
