#include "config.h"

#include <Arduino.h>
#include <EEPROM.h>

#include "arduino_debug.h"
#include "display.h"
#include "util.h"

const int c_rotary_encoder_switch_pin = 16;
const int c_rotary_encoder_dt_pin = 17;
const int c_rotary_encoder_clk_pin = 5;

SemaphoreHandle_t g_semaphore_configure = xSemaphoreCreateBinary();

void setup_eeprom() {
  EEPROM.begin(EEPROM_SIZE);

  default_initialize_config_values(true);
}

void default_initialize_config_values(bool force) {
  // The first address in EEPROM is a sentinel for whether or not
  // The config values have been default initialized.
  // This field will be checked first to prevent accidental overrides of
  // user-configured values. The when the "force" argument is true, config
  // values will be default initialized regardless of whether or not the
  // sentinel was set

  uint8_t initialization_sentinel = EEPROM.read(EEPROM_SENTINEL_ADDRESS);

  if (initialization_sentinel == EEPROM_INITIALIZED && !force) {
    debug_serial_println(
        "EEPROM was previously default initialised.\nSet force=true to "
        "force a default initialization and override existing config values.");
  } else {
    debug_serial_println("Default intializing EEPROM");

    // Add default configuration values here...
    EEPROM.write(EEPROM_HOUR_FORMAT_ADDRESS, EEPROM_HOUR_FORMAT_DEFAULT);

    // Set the sentinel value to show default initialization occurred
    EEPROM.write(EEPROM_SENTINEL_ADDRESS, EEPROM_INITIALIZED);

    EEPROM.commit();
  }

  if (ARDUINO_DEBUG) {
    // Print out the value of each EEPROM address
    Serial.println("EEPROM default initialised");
    Serial.println("\tAddress: value");
    for (size_t i = 0; i < EEPROM_SIZE; ++i) {
      size_t value = EEPROM.read(i);
      Serial.printf("\t%d: %d\n", i, value);
    }
  }
}

uint8_t get_config_value(uint8_t initial_value, uint8_t lower_bound,
                         uint8_t upper_bound) {
  uint16_t state = 0;
  int counter = initial_value;

  buzzer_click();

  // Blank the first displays
  for (int i = 0; i < 4; ++i) {
    shift_out_nixie_digit_single(NIXIE_BLANK_DIGIT);
  }
  shift_out_nixie_digit_pair(counter);
  shift_out_nixie_dots(0);

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
        if (counter > upper_bound) {
          counter = upper_bound;
        }
      } else {
        counter--;
        // Minimum value nixie tubes can display
        if (counter < lower_bound) {
          counter = lower_bound;
        }
      }

      buzzer_click();

      debug_serial_print(" -- Value: ");
      debug_serial_println(counter);

      // Blank the first displays
      for (int i = 0; i < 4; ++i) {
        shift_out_nixie_digit_single(NIXIE_BLANK_DIGIT);
      }
      shift_out_nixie_digit_pair(counter);
      shift_out_nixie_dots(0);
    }

    // Poll the semaphore to see if the encoder switch was pressed
    if (xSemaphoreTake(g_semaphore_configure, 0) == pdTRUE) {
      buzzer_click();
      return counter;
    }

    // Yield to higher priority tasks, otherwise keep running
    vTaskDelay(0);
  }
}
