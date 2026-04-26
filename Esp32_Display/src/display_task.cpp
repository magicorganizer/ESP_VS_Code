#include <Arduino.h>
#include "tasks.h"
#include <Adafruit_ST7789.h>

void Display_Main(void *pvParameters) {
  while (true) {
    // Display logic, for now just keep the image
    // Could add more display updates here

    lcd.drawRGBBitmap(0,0,imageData,LCD_WIDTH,LCD_HEIGHT);  // Displaying images on the screen

    vTaskDelay(10000 / portTICK_PERIOD_MS); // Delay 10 seconds
  }
}
