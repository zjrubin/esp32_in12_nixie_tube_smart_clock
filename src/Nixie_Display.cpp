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
  struct tm next_time;
  get_offset_time(&next_time, current_time, 1);

  uint8_t current_time_arr[num_display_digits];
  // We need to create an intermediate time struct. If any digit changed
  // from the current second to the next, that digit needs to fade to blank
  // first before the new digit appears
  uint8_t intermediate_blanked_arr[num_display_digits];
  uint8_t next_time_arr[num_display_digits];

  set_time_in_array(current_time_arr, current_time, twelve_hour_format);
  set_time_in_array(intermediate_blanked_arr, next_time, twelve_hour_format);
  set_time_in_array(next_time_arr, next_time, twelve_hour_format);

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

void Nixie_Display::display_date(const struct tm& time_info,
                                 uint8_t nixie_dots) {
  int month = time_info.tm_mon + 1;  // tm_month starts at 0
  m_digits[0] = TENS(month);
  m_digits[1] = ONES(month);

  m_digits[2] = TENS(time_info.tm_mday);
  m_digits[3] = ONES(time_info.tm_mday);

  m_digits[4] = TENS(time_info.tm_year);
  m_digits[5] = ONES(time_info.tm_year);

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

void Nixie_Display::display_slot_machine_cycle(const struct tm& current_time,
                                               bool twelve_hour_format) {
  static const int slot_machine_cycle_duration = 7;  // In seconds
  static const int slot_machine_num_phases = 10;
  const double slot_machine_phase_duration_ms =
      (slot_machine_cycle_duration * 1000) / (double)slot_machine_num_phases;

  struct tm end_time;
  get_offset_time(&end_time, current_time, slot_machine_cycle_duration);

  for (size_t i = slot_machine_num_phases; i > 0; --i) {
    slot_machine_cycle_phase(end_time, i, slot_machine_phase_duration_ms,
                             twelve_hour_format);
    reset_watchdog_timer();
  }
}

void Nixie_Display::slot_machine_cycle_phase(const struct tm& end_time,
                                             int num_cycling_digits,
                                             double phase_ms,
                                             bool twelve_hour_format) {
  // Show the time where the display is supposed to end.
  // This will lock in place the digits that are no longer cycling.
  // The digits that are still cycling will quickly be updated so that the
  // current time isn't seen
  set_time_in_array(m_digits, end_time, twelve_hour_format);
  show();

  static const int num_iterations = 10;
  const double iteration_duration_us =
      (phase_ms / (double)num_iterations) * 1000;

  for (size_t i = 0; i < num_iterations; ++i) {
    // The cascade behavior of this switch statement is desired here
    switch (num_cycling_digits) {
      default:  // Any number larger than 6 automatically cycles all digits
      case 6:
        m_digits[0] = CYCLE(m_digits[0]);
      case 5:
        m_digits[1] = CYCLE(m_digits[1]);
      case 4:
        m_digits[2] = CYCLE(m_digits[2]);
      case 3:
        m_digits[3] = CYCLE(m_digits[3]);
      case 2:
        m_digits[4] = CYCLE(m_digits[4]);
      case 1:
        m_digits[5] = CYCLE(m_digits[5]);
    }

    show();
    delayMicroseconds(iteration_duration_us);
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

void Nixie_Display::get_offset_time(struct tm* offset_time,
                                    const struct tm& current_time,
                                    int time_delta) {
  *offset_time = current_time;
  offset_time->tm_sec += time_delta;

  // mktime automatically normalizes the time (i.e. increments higher minutes,
  // hours, etc at 60 seconds)
  time_t next_time_epoch = mktime(offset_time);

  localtime_r(&next_time_epoch, offset_time);
}
