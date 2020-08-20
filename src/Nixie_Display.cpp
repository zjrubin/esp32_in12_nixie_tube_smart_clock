#include "Nixie_Display.h"

#include <Arduino.h>

#include "util.h"

const uint8_t Nixie_Display::nixie_digits[] = {
    NIXIE_ZERO,  NIXIE_ONE,  NIXIE_TWO,       NIXIE_THREE,
    NIXIE_FOUR,  NIXIE_FIVE, NIXIE_SIX,       NIXIE_SEVEN,
    NIXIE_EIGHT, NIXIE_NINE, NIXIE_BLANK_CODE};

const uint8_t Nixie_Display::nixie_dots[] = {
    NIXIE_DOTS_NONE,  NIXIE_DOTS_ALL, NIXIE_DOTS_LEFT,
    NIXIE_DOTS_RIGHT, NIXIE_DOTS_TOP, NIXIE_DOTS_BOTTOM};

const int Nixie_Display::clock_pin = 12;
const int Nixie_Display::latch_pin = 14;
const int Nixie_Display::output_enable_pin = 27;
const int Nixie_Display::data_pin = 26;

uint8_t Nixie_Display::m_digits[num_display_digits] = {
    NIXIE_ZERO, NIXIE_ZERO, NIXIE_ZERO, NIXIE_ZERO, NIXIE_ZERO, NIXIE_ZERO};
uint8_t Nixie_Display::m_dots = NIXIE_DOTS_ALL;

SemaphoreHandle_t Nixie_Display::display_mutex = NULL;

void Nixie_Display::setup_nixie_display() {
  // Set the pin modes
  pinMode(clock_pin, OUTPUT);
  pinMode(latch_pin, OUTPUT);
  pinMode(output_enable_pin, OUTPUT);
  pinMode(data_pin, OUTPUT);

  digitalWrite(output_enable_pin, LOW);  // Enables output

  display_mutex = xSemaphoreCreateMutex();
}

void Nixie_Display::smooth_display_time(const struct tm& current_time,
                                        bool twelve_hour_format,
                                        uint8_t nixie_dots) {
  struct tm tmp_next_time = current_time;
  tmp_next_time.tm_sec += 1;

  // mktime automatically normalizes the time (i.e. increments higher minutes,
  // hours, etc at 60 seconds)
  time_t next_time_epoch = mktime(&tmp_next_time);
  struct tm* next_time = localtime(&next_time_epoch);

  uint8_t current_time_arr[num_display_digits];
  // We need to create an intermediate time struct. If any digit changed
  // from the current second to the next, that digit needs to fade to blank
  // first before the new digit appears
  uint8_t intermediate_blanked_arr[num_display_digits];
  uint8_t next_time_arr[num_display_digits];

  set_time_in_array(current_time_arr, current_time, twelve_hour_format);
  set_time_in_array(intermediate_blanked_arr, *next_time, twelve_hour_format);
  set_time_in_array(next_time_arr, *next_time, twelve_hour_format);

  // Determine which digits changed from the current time to the next time,
  // and thus need to be blanked
  for (size_t i = 0; i < num_display_digits; ++i) {
    if (current_time_arr[i] != next_time_arr[i]) {
      intermediate_blanked_arr[i] = NIXIE_BLANK_POS;
    }
  }

  m_dots = nixie_dots;

  // The transition happens imperceptibly faster than 1 second.
  // this ensures that the display time task can call vTaskDelayUntil
  // and wake up exactly 1 second after it was initally called.
  // This prevents time drift appearing on the display.
  smooth_display_transition(current_time_arr, intermediate_blanked_arr,
                            NIXIE_SMOOTH_TRANSITION_TIME_MS / 2);
  smooth_display_transition(intermediate_blanked_arr, next_time_arr,
                            NIXIE_SMOOTH_TRANSITION_TIME_MS / 2);
}

void Nixie_Display::smooth_display_transition(
    const uint8_t current_digits[num_display_digits],
    const uint8_t next_digits[num_display_digits], size_t transition_time_ms) {
  size_t multiplex_count = 100;
  double single_digit_transition_time =
      transition_time_ms / (double)multiplex_count;

  for (size_t i = 0; i < multiplex_count; ++i) {
    double current_digit_display_proportion =
        0.5 * (cos(PI * (i / (double)multiplex_count)) + 1);
    double next_digit_display_proportion = 1 - current_digit_display_proportion;

    memcpy(m_digits, current_digits, sizeof(m_digits));
    show();
    delayMicroseconds(
        (current_digit_display_proportion * single_digit_transition_time) *
        MILLISECOND_TO_MICROSECONDS);

    memcpy(m_digits, next_digits, sizeof(m_digits));
    show();
    delayMicroseconds(
        (next_digit_display_proportion * single_digit_transition_time) *
        MILLISECOND_TO_MICROSECONDS);
  }
}

void Nixie_Display::display_time(const struct tm& time_info,
                                 bool twelve_hour_format, uint8_t nixie_dots) {
  set_time_in_array(m_digits, time_info, twelve_hour_format);

  m_dots = nixie_dots;

  show();
}

void Nixie_Display::display_config_value(uint8_t option_number, uint8_t value) {
  m_digits[0] = TENS(option_number);
  m_digits[1] = ONES(option_number);

  m_digits[2] = NIXIE_BLANK_POS;
  m_digits[3] = NIXIE_BLANK_POS;

  // Blank the leading digit of the value if it is zero
  uint8_t value_tens = TENS(value);
  m_digits[4] = value_tens == 0 ? NIXIE_BLANK_POS : value_tens;
  m_digits[5] = ONES(value);

  // Leave dots the same

  show();
}

void Nixie_Display::display_slot_machine_cycle(struct tm* time_info) {
  static const int slot_machine_cycle_duration = 7;  // In seconds
  static const size_t slot_machine_cycle_duration_milliseconds =
      1000 * 1000 * slot_machine_cycle_duration;

  time_t time = mktime(time_info);
  time += slot_machine_cycle_duration;
  // struct tm* end_time = localtime(&time);

  static const int for_loop_iterations = 100;
  static const int iteration_duration =
      slot_machine_cycle_duration_milliseconds / for_loop_iterations;

  for (int i = 0; i < for_loop_iterations; ++i) {
    for (size_t j = 0; j < num_display_digits; ++j) {
      m_digits[j] = (m_digits[j] + 1) % 10;
    }
    show();
    reset_watchdog_timer();
    delayMicroseconds(iteration_duration);
  }
}

void Nixie_Display::show() const {
  digitalWrite(latch_pin, LOW);

  // Shift out each pair of digits
  for (size_t i = 0; i < num_display_digits - 1; i += 2) {
    shiftOut(data_pin, clock_pin, LSBFIRST,
             LEFT_DISPLAY(nixie_digits[m_digits[i]]) |
                 RIGHT_DISPLAY(nixie_digits[m_digits[i + 1]]));
  }

  // Shift out the dot separators
  shiftOut(data_pin, clock_pin, MSBFIRST, m_dots);

  digitalWrite(latch_pin, HIGH);
}

void Nixie_Display::set_time_in_array(uint8_t array[num_display_digits],
                                      const struct tm& time_info,
                                      bool twelve_hour_format) {
  uint8_t hours = time_info.tm_hour;
  if (twelve_hour_format) {
    hours = convert_24_hour_to_12_hour(hours);
  }

  array[0] = TENS(hours);
  array[1] = ONES(hours);

  array[2] = TENS(time_info.tm_min);
  array[3] = ONES(time_info.tm_min);

  array[4] = TENS(time_info.tm_sec);
  array[5] = ONES(time_info.tm_sec);
}
