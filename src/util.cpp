#include "util.h"

#include <Arduino.h>

const size_t c_minute_freertos = (60 * (1024 / portTICK_PERIOD_MS));
const int c_buzzer_pin = 18;

void buzzer_click() {
  digitalWrite(c_buzzer_pin, HIGH);
  delay(1);
  digitalWrite(c_buzzer_pin, LOW);
}
