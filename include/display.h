#pragma once

#include <stddef.h>
#include <stdint.h>

#define NIXIE_ZERO 0b0000
#define NIXIE_ONE 0b1000
#define NIXIE_TWO 0b0100
#define NIXIE_THREE 0b1100
#define NIXIE_FOUR 0b0010
#define NIXIE_FIVE 0b1010
#define NIXIE_SIX 0b0110
#define NIXIE_SEVEN 0b1110
#define NIXIE_EIGHT 0b0001
#define NIXIE_NINE 0b1001
#define NIXIE_BLANK_CODE 0b1111

#define NUM_NIXIE_DIGITS 10

#define LEFT_DISPLAY(nixie_digit) (nixie_digit << 4)
#define RIGHT_DISPLAY(nixie_digit) (nixie_digit)
#define DUAL_DISPLAY(left_nixie_digit, right_nixie_digit) \
  (LEFT_DISPLAY(left_nixie_digit) | RIGHT_DISPLAY(right_nixie_digit))

const uint8_t nixie_digits[] = {NIXIE_ZERO,  NIXIE_ONE,       NIXIE_TWO,
                                NIXIE_THREE, NIXIE_FOUR,      NIXIE_FIVE,
                                NIXIE_SIX,   NIXIE_SEVEN,     NIXIE_EIGHT,
                                NIXIE_NINE,  NIXIE_BLANK_CODE};

#define NIXIE_BLANK NUM_ELEMENTS(nixie_digits)

extern const int c_clock_pin;
extern const int c_latch_pin;
extern const int c_output_enable_pin;
extern const int c_data_pin;

extern const int c_upper_left_dot;

void smooth_transition_nixie_digit(uint8_t next_digit, uint8_t current_digit,
                                   size_t transition_time_ms);

void _smooth_transition_helper(uint8_t next_digit, uint8_t current_digit,
                               size_t transition_time_ms);

void shift_out_nixie_digit(uint8_t digit);
