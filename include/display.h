#pragma once

#include <stddef.h>
#include <stdint.h>

#define ONES(x) x % 10
#define TENS(x) (x / 10) % 10
#define HUNDREDS(x) (x / 100) % 10
#define THOUSANDS(x) (x / 1000) % 10

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

#define NIXIE_DOTS_NONE 0b0000
#define NIXIE_DOTS_ALL 0b1111
#define NIXIE_DOTS_LEFT 0b1100
#define NIXIE_DOTS_RIGHT 0b0011
#define NIXIE_DOTS_TOP 0b1010
#define NIXIE_DOTS_BOTTOM 0b0101

#define NUM_NIXIE_DIGITS 10
#define NIXIE_BLANK_POS 10
#define NIXIE_BLANK_DIGIT -1  // Digit that corresponds to a blank nixie display

#define LEFT_DISPLAY(nixie_digit) (nixie_digit << 4)
#define RIGHT_DISPLAY(nixie_digit) (nixie_digit)
#define DUAL_DISPLAY(left_nixie_digit, right_nixie_digit) \
  (LEFT_DISPLAY(left_nixie_digit) | RIGHT_DISPLAY(right_nixie_digit))

extern const uint8_t nixie_digits[];

extern const uint8_t nixie_dots[];

#define NIXIE_BLANK NUM_ELEMENTS(nixie_digits)

extern const int c_clock_pin;
extern const int c_latch_pin;
extern const int c_output_enable_pin;
extern const int c_data_pin;

extern const int c_upper_left_dot;

void shift_out_time(const struct tm& time_info, bool twelve_hour_format = true,
                    uint8_t nixie_dots = NIXIE_DOTS_ALL);

void smooth_transition_nixie_digit(uint8_t next_digit, uint8_t current_digit,
                                   size_t transition_time_ms);

void _smooth_transition_helper(uint8_t next_digit, uint8_t current_digit,
                               size_t transition_time_ms);

// Digit must be in range [0, 99]
void shift_out_nixie_digit_pair(uint8_t digit, bool blank_leading_zero = false);

// Digit must be in range [0, 9]
void shift_out_nixie_digit_single(uint8_t digit);

void shift_out_nixie_dots(uint8_t dots);
