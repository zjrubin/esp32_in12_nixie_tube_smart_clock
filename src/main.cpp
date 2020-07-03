#include <Arduino.h>

const int c_led_pin = 2;

const int c_led_blink_delay = 500;

void setup()
{
  pinMode(c_led_pin, OUTPUT);
}

void loop()
{
  digitalWrite(c_led_pin, !digitalRead(c_led_pin));
  delay(c_led_blink_delay);
}
