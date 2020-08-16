#pragma once

#include <FreeRTOS.h>
#include <stdint.h>

#define EEPROM_SIZE 10
#define EEPROM_SENTINEL_ADDRESS 0
#define EEPROM_INITIALIZED 1

#define EEPROM_HOUR_FORMAT_ADDRESS 1
#define EEPROM_HOUR_FORMAT_OPTION_1 12
#define EEPROM_HOUR_FORMAT_OPTION_2 24
#define EEPROM_HOUR_FORMAT_DEFAULT EEPROM_HOUR_FORMAT_OPTION_1

extern const int c_rotary_encoder_switch_pin;
extern const int c_rotary_encoder_dt_pin;
extern const int c_rotary_encoder_clk_pin;

extern SemaphoreHandle_t g_semaphore_configure;

void setup_eeprom();

void default_initialize_config_values(bool force = false);

uint8_t get_config_value(uint8_t initial_value, uint8_t lower_bound = 0,
                         uint8_t upper_bound = 1);
