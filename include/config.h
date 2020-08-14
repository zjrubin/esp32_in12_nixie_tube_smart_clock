#pragma once

#include <FreeRTOS.h>
#include <stdint.h>

extern const int c_rotary_encoder_switch_pin;
extern const int c_rotary_encoder_dt_pin;
extern const int c_rotary_encoder_clk_pin;

extern SemaphoreHandle_t g_semaphore_configure;

void setup_eeprom();

void default_initialize_config_values(bool force = false);

uint8_t get_config_value(uint8_t initial_value);
