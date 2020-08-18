#pragma once

#include <FreeRTOS.h>
#include <stdint.h>

#define EEPROM_SIZE 10
#define EEPROM_SENTINEL_ADDRESS 0
#define EEPROM_INITIALIZED 1

#define EEPROM_12_HOUR_FORMAT_ADDRESS 1
#define EEPROM_12_HOUR_FORMAT_DEFAULT 1
#define EEPROM_12_HOUR_FORMAT_LOWER_BOUND 0
#define EEPROM_12_HOUR_FORMAT_UPPER_BOUND 1

#define EEPROM_SLOT_MACHINE_CYCLE_FREQUENCY_ADDRESS 2
#define EEPROM_SLOT_MACHINE_CYCLE_FREQUENCY_DEFAULT 30  // In minutes
#define EEPROM_SLOT_MACHINE_CYCLE_FREQUENCY_LOWER_BOUND 1
#define EEPROM_SLOT_MACHINE_CYCLE_FREQUENCY_UPPER_BOUND 99

extern const int c_rotary_encoder_switch_pin;
extern const int c_rotary_encoder_dt_pin;
extern const int c_rotary_encoder_clk_pin;

extern SemaphoreHandle_t g_semaphore_configure;

void setup_eeprom();

void default_initialize_config_values(bool force = false);

void handle_configuration();

uint8_t get_config_value(uint8_t option_number, uint8_t initial_value,
                         uint8_t lower_bound, uint8_t upper_bound);

typedef struct {
  uint8_t option_number;
  uint8_t initial_value;
  uint8_t lower_bound;
  uint8_t upper_bound;
} eeprom_option_t;
