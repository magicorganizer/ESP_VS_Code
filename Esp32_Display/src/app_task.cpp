#include <Arduino.h>
#include "tasks.h"

void App_Main(void *pvParameters) {
  while (true) {
    handle_timers();
    vTaskDelay(10 / portTICK_PERIOD_MS); // Small delay
  }
}
