#include "config.h"

#include <Arduino.h>

#include "arduino_debug.h"
#include "display.h"
#include "util.h"

const int c_rotary_encoder_switch_pin = 16;
const int c_rotary_encoder_dt_pin = 17;
const int c_rotary_encoder_clk_pin = 5;

SemaphoreHandle_t g_semaphore_configure = xSemaphoreCreateBinary();

uint8_t get_config_value(uint8_t initial_value) {
  uint16_t state = 0;
  int counter = initial_value;

  buzzer_click();

  while (true) {
    // Feed the watchdog timer so that the MCU isn't reset
    reset_watchdog_timer();

    // Debounce filtering for the rotary encoder
    state = (state << 1) | digitalRead(c_rotary_encoder_clk_pin) | 0xe000;

    if (state == 0xf000) {
      state = 0x0000;
      if (digitalRead(c_rotary_encoder_dt_pin)) {
        counter++;
        // Maximum value 1 nixie tube can display
        if (counter > 9) {
          counter = 9;
        }
      } else {
        counter--;
        // Minimum value nixie tubes can display
        if (counter < 0) {
          counter = 0;
        }
      }

      buzzer_click();

      debug_serial_print(" -- Value: ");
      debug_serial_println(counter);
    }
    shift_out_nixie_digit(counter);

    // Poll the semaphore to see if the encoder switch was pressed
    if (xSemaphoreTake(g_semaphore_configure, 0) == pdTRUE) {
      buzzer_click();
      return counter;
    }

    // Yield to higher priority tasks, otherwise keep running
    vTaskDelay(0);
  }
}
