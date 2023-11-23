#pragma once

#include <FreeRTOS.h>
#include <stdint.h>

class Nixie_Display;

#define EEPROM_SIZE 10

#define EEPROM_SENTINEL_ADDRESS 0
#define EEPROM_INITIALIZED 1

#define EEPROM_12_HOUR_FORMAT_ADDRESS 1
#define EEPROM_12_HOUR_FORMAT_DEFAULT 1
#define EEPROM_12_HOUR_FORMAT_LOWER_BOUND 0
#define EEPROM_12_HOUR_FORMAT_UPPER_BOUND 1

#define EEPROM_DATE_DISPLAY_FREQUENCY_ADDRESS 2
#define EEPROM_DATE_DISPLAY_FREQUENCY_DEFAULT 5
#define EEPROM_DATE_DISPLAY_FREQUENCY_LOWER_BOUND 0
#define EEPROM_DATE_DISPLAY_FREQUENCY_UPPER_BOUND 99

#define EEPROM_SLOT_MACHINE_CYCLE_FREQUENCY_ADDRESS 3
#define EEPROM_SLOT_MACHINE_CYCLE_FREQUENCY_DEFAULT 10  // In minutes
#define EEPROM_SLOT_MACHINE_CYCLE_FREQUENCY_LOWER_BOUND 1
#define EEPROM_SLOT_MACHINE_CYCLE_FREQUENCY_UPPER_BOUND 60

#define EEPROM_SPECIAL_MODES_ADDRESS 4
#define EEPROM_SPECIAL_MODES_DEFAULT 0
#define EEPROM_SPECIAL_MODES_LOWER_BOUND 0
#define EEPROM_SPECIAL_MODES_UPPER_BOUND 1

#define EEPROM_LOCAL_TEMPERATURE_DISPLAY_FREQUENCY_ADDRESS 5
#define EEPROM_LOCAL_TEMPERATURE_DISPLAY_FREQUENCY_DEFAULT 5
#define EEPROM_LOCAL_TEMPERATURE_DISPLAY_FREQUENCY_LOWER_BOUND 5
#define EEPROM_LOCAL_TEMPERATURE_DISPLAY_FREQUENCY_UPPER_BOUND 99

// For 1 hour, the nixie display will cycle all of its digits very frequently.
// By default, this period is scheduled for the early morning as to not be
// inconvenient or distracting.
#define MANDATORY_CATHODE_POISONING_PREVENTION_HOUR 3

extern const int c_rotary_encoder_switch_pin;
extern const int c_rotary_encoder_dt_pin;
extern const int c_rotary_encoder_clk_pin;

extern SemaphoreHandle_t g_semaphore_configure;

void setup_eeprom();

void default_initialize_config_values(bool force = false);

void handle_configuration();

uint8_t get_config_value(uint8_t option_number, uint8_t initial_value,
                         uint8_t lower_bound, uint8_t upper_bound,
                         void (Nixie_Display::*display_handler)(uint8_t,
                                                                uint8_t));

typedef struct {
  uint8_t option_number;
  uint8_t initial_value;
  uint8_t lower_bound;
  uint8_t upper_bound;
  TaskHandle_t *task_handle;
} eeprom_option_t;
