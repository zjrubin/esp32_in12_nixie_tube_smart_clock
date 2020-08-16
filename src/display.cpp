#include "display.h"

#include <Arduino.h>

#include "arduino_debug.h"
#include "util.h"

const int c_clock_pin = 12;
const int c_latch_pin = 14;
const int c_output_enable_pin = 27;
const int c_data_pin = 26;

const int c_upper_left_dot = 25;

const uint8_t nixie_digits[] = {NIXIE_ZERO,  NIXIE_ONE,       NIXIE_TWO,
                                NIXIE_THREE, NIXIE_FOUR,      NIXIE_FIVE,
                                NIXIE_SIX,   NIXIE_SEVEN,     NIXIE_EIGHT,
                                NIXIE_NINE,  NIXIE_BLANK_CODE};

extern const uint8_t nixie_dots[] = {NIXIE_DOTS_NONE, NIXIE_DOTS_ALL,
                                     NIXIE_DOTS_LEFT, NIXIE_DOTS_RIGHT,
                                     NIXIE_DOTS_TOP,  NIXIE_DOTS_BOTTOM};

static void shift_out_four_bits(uint8_t dataPin, uint8_t clockPin,
                                uint8_t bitOrder, uint8_t val);
static inline uint8_t convert_24_hour_to_12_hour(uint8_t hours);

void shift_out_time(const struct tm& time_info, bool twelve_hour_format,
                    uint8_t nixie_dots) {
  uint8_t hours = time_info.tm_hour;
  if (twelve_hour_format) {
    hours = convert_24_hour_to_12_hour(hours);
  }

  uint8_t minutes = time_info.tm_min;
  uint8_t seconds = time_info.tm_sec;

  shift_out_nixie_digit_pair(hours);
  shift_out_nixie_digit_pair(minutes);
  shift_out_nixie_digit_pair(seconds);

  // for (uint8_t i = 0; i < 10; ++i) {
  //   debug_serial_println(i);

  //   digitalWrite(c_latch_pin, LOW);

  //   uint8_t display_digit =
  //       LEFT_DISPLAY(nixie_digits[i]) | RIGHT_DISPLAY(nixie_digits[i]);

  //   shiftOut(c_data_pin, c_clock_pin, LSBFIRST, display_digit);
  //   shiftOut(c_data_pin, c_clock_pin, LSBFIRST, display_digit);
  //   shiftOut(c_data_pin, c_clock_pin, LSBFIRST, display_digit);

  //   // Dots
  //   display_digit =
  //       LEFT_DISPLAY(nixie_digits[11]) | RIGHT_DISPLAY(nixie_digits[11]);

  //   shiftOut(c_data_pin, c_clock_pin, LSBFIRST, display_digit);

  //   digitalWrite(c_latch_pin, HIGH);

  //   vTaskDelay(2000 / portTICK_PERIOD_MS);
  // }

  // Set the dot seperators
  shift_out_nixie_dots(nixie_dots);
}

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

    shift_out_nixie_digit_pair(current_digit);
    delayMicroseconds(
        (current_digit_display_proportion * single_digit_transition_time) *
        MILLISECOND_TO_MICROSECONDS);

    shift_out_nixie_digit_pair(next_digit);
    delayMicroseconds(
        (next_digit_display_proportion * single_digit_transition_time) *
        MILLISECOND_TO_MICROSECONDS);
  }
}

void shift_out_nixie_digit_pair(uint8_t digit, bool blank_leading_zero) {
  uint8_t left_digit = TENS(digit);
  if (left_digit == 0 && blank_leading_zero) {
    left_digit = NIXIE_BLANK_POS;
  }
  uint8_t right_digit = ONES(digit);

  // Need to shift out least significant bits first
  shift_out_nixie_digit_single(right_digit);
  shift_out_nixie_digit_single(left_digit);
}

void shift_out_nixie_digit_single(uint8_t digit) {
  digitalWrite(c_latch_pin, LOW);

  uint8_t nixie_code =
      digit == NIXIE_BLANK_DIGIT ? NIXIE_BLANK_CODE : nixie_digits[digit];

  shift_out_four_bits(c_data_pin, c_clock_pin, LSBFIRST, nixie_code);

  digitalWrite(c_latch_pin, HIGH);
}

void shift_out_nixie_dots(uint8_t dots) {
  digitalWrite(c_latch_pin, LOW);

  // The dots correspond to QA, QB, QC, and QD of the shift register
  // shiftOut(c_data_pin, c_clock_pin, LSBFIRST, (dots <<= 4));
  shiftOut(c_data_pin, c_clock_pin, MSBFIRST, dots);
  // shiftOut(c_data_pin, c_clock_pin, LSBFIRST, 0b10100000);

  digitalWrite(c_latch_pin, HIGH);
}

static void shift_out_four_bits(uint8_t dataPin, uint8_t clockPin,
                                uint8_t bitOrder, uint8_t val) {
  uint8_t i;

  for (i = 0; i < 4; i++) {
    if (bitOrder == LSBFIRST) {
      digitalWrite(dataPin, !!(val & (1 << i)));
    } else {
      digitalWrite(dataPin, !!(val & (1 << (7 - i))));
    }

    digitalWrite(clockPin, HIGH);
    digitalWrite(clockPin, LOW);
  }
}

static inline uint8_t convert_24_hour_to_12_hour(uint8_t hours) {
  hours = hours % 12;
  return hours == 0 ? 12 : hours;
}
