#include "config.h"

#include <Arduino.h>
#include <EEPROM.h>

#include "Nixie_Display.h"
#include "arduino_debug.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "tasks.h"
#include "util.h"

const int c_rotary_encoder_switch_pin = 16;
const int c_rotary_encoder_dt_pin = 17;
const int c_rotary_encoder_clk_pin = 5;

static const eeprom_option_t c_eeprom_options[] = {
    // A NULL value for task_handle means that config will disallow vTaskSuspend
    // and vTaskResume and treat all values as potential config values.
    // A non-NULL task_handle will allow config to suspend the task if the
    // entered value is zero and enable the task if the entered value is
    // non-zero
    {EEPROM_12_HOUR_FORMAT_ADDRESS, EEPROM_12_HOUR_FORMAT_DEFAULT,
     EEPROM_12_HOUR_FORMAT_LOWER_BOUND, EEPROM_12_HOUR_FORMAT_UPPER_BOUND,
     NULL},
    {EEPROM_DATE_DISPLAY_FREQUENCY_ADDRESS,
     EEPROM_DATE_DISPLAY_FREQUENCY_DEFAULT,
     EEPROM_DATE_DISPLAY_FREQUENCY_LOWER_BOUND,
     EEPROM_DATE_DISPLAY_FREQUENCY_UPPER_BOUND, &g_task_display_date_handle},
    {EEPROM_SLOT_MACHINE_CYCLE_FREQUENCY_ADDRESS,
     EEPROM_SLOT_MACHINE_CYCLE_FREQUENCY_DEFAULT,
     EEPROM_SLOT_MACHINE_CYCLE_FREQUENCY_LOWER_BOUND,
     EEPROM_SLOT_MACHINE_CYCLE_FREQUENCY_UPPER_BOUND, NULL},
    {EEPROM_SPECIAL_MODES_ADDRESS, EEPROM_SPECIAL_MODES_DEFAULT,
     EEPROM_SPECIAL_MODES_LOWER_BOUND, EEPROM_SPECIAL_MODES_UPPER_BOUND,
     &g_task_special_modes_handle},
    {EEPROM_LOCAL_TEMPERATURE_DISPLAY_FREQUENCY_ADDRESS,
     EEPROM_LOCAL_TEMPERATURE_DISPLAY_FREQUENCY_DEFAULT,
     EEPROM_LOCAL_TEMPERATURE_DISPLAY_FREQUENCY_LOWER_BOUND,
     EEPROM_LOCAL_TEMPERATURE_DISPLAY_FREQUENCY_UPPER_BOUND,
     &g_task_display_local_temperature_handle},
};

SemaphoreHandle_t g_semaphore_configure = xSemaphoreCreateBinary();

static void set_eeprom_config_value(uint8_t option_number,
                                    uint8_t initial_value, uint8_t lower_bound,
                                    uint8_t upper_bound,
                                    TaskHandle_t *task_handle);

void setup_eeprom() {
  EEPROM.begin(EEPROM_SIZE);

  default_initialize_config_values(false);
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

    // Set default EEPROM values
    for (size_t i = 0; i < NUM_ELEMENTS(c_eeprom_options); ++i) {
      EEPROM.write(c_eeprom_options[i].option_number,
                   c_eeprom_options[i].initial_value);
    }

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

void handle_configuration() {
  debug_serial_println("Configuration Menu:");
  // Go through all the entries in EEPROM to configure them
  // Note: for condition is "NUM_EEPROM_CONFIG_VALUES + 1" since i starts at 1,
  // The first address fro config values
  for (size_t i = 0; i < NUM_ELEMENTS(c_eeprom_options); ++i) {
    set_eeprom_config_value(
        c_eeprom_options[i].option_number, c_eeprom_options[i].initial_value,
        c_eeprom_options[i].lower_bound, c_eeprom_options[i].upper_bound,
        c_eeprom_options[i].task_handle);
  }
  EEPROM.commit();  // Still need to commit the changes
}

static void set_eeprom_config_value(uint8_t option_number,
                                    uint8_t initial_value, uint8_t lower_bound,
                                    uint8_t upper_bound,
                                    TaskHandle_t *task_handle) {
  uint8_t config_value = EEPROM.read(option_number);
  debug_serial_printf("\tCurrent option: %d value: %d\n", option_number,
                      config_value);
  config_value =
      get_config_value(option_number, config_value, lower_bound, upper_bound,
                       &Nixie_Display::display_config_value);

  if (task_handle) {
    config_value ? vTaskResume(*task_handle) : vTaskSuspend(*task_handle);
  }

  debug_serial_printf("\tStoring option: %d value: %d\n", option_number,
                      config_value);
  EEPROM.write(option_number, config_value);
}

uint8_t get_config_value(uint8_t option_number, uint8_t initial_value,
                         uint8_t lower_bound, uint8_t upper_bound,
                         void (Nixie_Display::*display_handler)(uint8_t,
                                                                uint8_t)) {
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
    }

    (Nixie_Display::get_instance().*display_handler)(option_number, counter);

    // Poll the semaphore to see if the encoder switch was pressed
    if (xSemaphoreTake(g_semaphore_configure, 0) == pdTRUE) {
      buzzer_click();
      return counter;
    }

    // Yield to higher priority tasks, otherwise keep running
    vTaskDelay(0);
  }
}
