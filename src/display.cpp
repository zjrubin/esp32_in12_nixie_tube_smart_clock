#include "display.h"

#include <Arduino.h>

#include "util.h"

const int c_clock_pin = 12;
const int c_latch_pin = 14;
const int c_output_enable_pin = 27;
const int c_data_pin = 26;

const int c_upper_left_dot = 25;

void smooth_transition_nixie_digit(uint8_t next_digit, uint8_t current_digit,
                                   size_t transition_time_ms) {
  _smooth_transition_helper(NIXIE_BLANK, current_digit,
                            transition_time_ms / 2.0);
  _smooth_transition_helper(next_digit, NIXIE_BLANK, transition_time_ms / 2.0);
}

void _smooth_transition_helper(uint8_t next_digit, uint8_t current_digit,
                               size_t transition_time_ms) {
  size_t multiplex_count = 100;
  double single_digit_transition_time =
      transition_time_ms / (double)multiplex_count;

  for (size_t i = 0; i < multiplex_count; ++i) {
    double current_digit_display_proportion =
        0.5 * (cos(PI * (i / (double)multiplex_count)) + 1);
    double next_digit_display_proportion = 1 - current_digit_display_proportion;

    shift_out_nixie_digit(current_digit);
    delayMicroseconds(
        (current_digit_display_proportion * single_digit_transition_time) *
        MILLISECOND_TO_MICROSECONDS);

    shift_out_nixie_digit(next_digit);
    delayMicroseconds(
        (next_digit_display_proportion * single_digit_transition_time) *
        MILLISECOND_TO_MICROSECONDS);
  }
}

void shift_out_nixie_digit(uint8_t digit) {
  digitalWrite(c_latch_pin, LOW);

  uint8_t display_digit = LEFT_DISPLAY(nixie_digits[digit]);

  shiftOut(c_data_pin, c_clock_pin, LSBFIRST, display_digit);

  digitalWrite(c_latch_pin, HIGH);
}
