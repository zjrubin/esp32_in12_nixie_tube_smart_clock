#pragma once

#include <FreeRTOS.h>
#include <stdint.h>

#define EEPROM_SIZE 10

extern const int c_rotary_encoder_switch_pin;
extern const int c_rotary_encoder_dt_pin;
extern const int c_rotary_encoder_clk_pin;

extern SemaphoreHandle_t g_semaphore_configure;

uint8_t get_config_value(uint8_t initial_value);
