#include "Nixie_Display.h"

#include <Arduino.h>

#include "util.h"

const uint8_t Nixie_Display::nixie_digits[NUM_NIXIE_DIGITS] = {
    NIXIE_ZERO,  NIXIE_ONE,  NIXIE_TWO,       NIXIE_THREE,
    NIXIE_FOUR,  NIXIE_FIVE, NIXIE_SIX,       NIXIE_SEVEN,
    NIXIE_EIGHT, NIXIE_NINE, NIXIE_BLANK_CODE};

const int Nixie_Display::clock_pin = 12;
const int Nixie_Display::latch_pin = 14;
const int Nixie_Display::output_enable_pin = 27;
const int Nixie_Display::data_pin = 26;

uint8_t Nixie_Display::m_digits[num_display_digits] = {NIXIE_ZERO};
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

  uint8_t hours = next_time.tm_hour;
  if (twelve_hour_format) {
    hours = convert_24_hour_to_12_hour(hours);
  }

  // The transition happens imperceptibly faster than 1 second.
  // this ensures that the display time task can call vTaskDelayUntil
  // and wake up exactly 1 second after it was initially called.
  // This prevents time drift appearing on the display.
  smooth_display_value(NIXIE_SMOOTH_TRANSITION_TIME_MS, hours, next_time.tm_min,
                       next_time.tm_sec, nixie_dots, false);
}

void Nixie_Display::smooth_display_value(size_t transition_time_ms,
                                         int8_t hours, int8_t minutes,
                                         int8_t seconds, uint8_t nixie_dots,
                                         bool blank_all) {
  uint8_t current_value_arr[num_display_digits];
  // We need to create an intermediate time struct. If any digit changed
  // from the current second to the next, that digit needs to fade to blank
  // first before the new digit appears
  uint8_t intermediate_blanked_arr[num_display_digits];
  uint8_t next_value_arr[num_display_digits];

  memcpy(current_value_arr, m_digits, sizeof(m_digits));
  set_digit_array_from_value(next_value_arr, hours, minutes, seconds);

  uint8_t current_dots = m_dots;
  uint8_t intermediate_dots = nixie_dots;

  if (blank_all) {
    memset(intermediate_blanked_arr, NIXIE_BLANK_POS,
           sizeof(intermediate_blanked_arr));
    intermediate_dots = NIXIE_DOTS_NONE;
  } else {
    // Determine which digits changed from the current time to the next time,
    // and thus need to be blanked
    set_digit_array_from_value(intermediate_blanked_arr, hours, minutes,
                               seconds);
    for (size_t i = 0; i < num_display_digits; ++i) {
      if (current_value_arr[i] != next_value_arr[i]) {
        intermediate_blanked_arr[i] = NIXIE_BLANK_POS;
      }
    }
    if (current_dots != nixie_dots) {
      intermediate_dots = NIXIE_DOTS_NONE;
    }
  }

  smooth_display_transition(current_value_arr, intermediate_blanked_arr,
                            current_dots, intermediate_dots,
                            transition_time_ms / 2);
  smooth_display_transition(intermediate_blanked_arr, next_value_arr,
                            intermediate_dots, nixie_dots,
                            transition_time_ms / 2);
}

void Nixie_Display::smooth_display_transition(
    const uint8_t current_digits[num_display_digits],
    const uint8_t next_digits[num_display_digits], uint8_t current_nixie_dots,
    uint8_t next_nixie_dots, size_t transition_time_ms) {
  size_t multiplex_count = 100;
  double single_digit_transition_time =
      transition_time_ms / (double)multiplex_count;

  for (size_t i = 0; i < multiplex_count; ++i) {
    double current_digit_display_proportion =
        0.5 * (cos(PI * (i / (double)multiplex_count)) + 1);
    double next_digit_display_proportion = 1 - current_digit_display_proportion;

    memcpy(m_digits, current_digits, sizeof(m_digits));
    m_dots = current_nixie_dots;
    show();
    delayMicroseconds(
        (current_digit_display_proportion * single_digit_transition_time) *
        MILLISECOND_TO_MICROSECONDS);

    memcpy(m_digits, next_digits, sizeof(m_digits));
    m_dots = next_nixie_dots;
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
  uint8_t month = time_info.tm_mon + 1;  // tm_month starts at 0

  set_digit_array_from_value(m_digits, month, time_info.tm_mday,
                             time_info.tm_year);

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

void Nixie_Display::display_timer_select(uint8_t digit_pair_pos,
                                         uint8_t value) {
  // digit_pair_pos: 0 == HOURS, 1 == MINTUES, 2 == SECONDS

  size_t left_digit = 0;
  size_t right_digit = 0;

  // Keep all the other digits the same
  switch (digit_pair_pos) {
    case 0:  // HOURS
      left_digit = 0;
      right_digit = 1;
      break;

    case 1:  // MINUTES
      left_digit = 2;
      right_digit = 3;
      break;

    case 2:  // SECONDS
      left_digit = 4;
      right_digit = 5;
      break;

    default:
      break;
  }

  uint8_t value_tens = TENS(value);
  m_digits[left_digit] = value_tens == 0 ? NIXIE_BLANK_POS : value_tens;
  m_digits[right_digit] = ONES(value);

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

void Nixie_Display::display_value(uint8_t hours, uint8_t minutes,
                                  uint8_t seconds, uint8_t nixie_dots) {
  set_digit_array_from_value(m_digits, hours, minutes, seconds);

  m_dots = nixie_dots;

  show();
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

  set_digit_array_from_value(array, hours, time_info.tm_min, time_info.tm_sec);
}

void Nixie_Display::get_current_display(uint8_t* hours, uint8_t* minutes,
                                        uint8_t* seconds, uint8_t* dots) const {
  *hours = (10 * m_digits[0]) + m_digits[1];
  *minutes = (10 * m_digits[2]) + m_digits[3];
  *seconds = (10 * m_digits[4]) + m_digits[5];
  *dots = m_dots;
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

void Nixie_Display::set_digit_array_from_value(
    uint8_t digit_array[Nixie_Display::num_display_digits], int8_t hours,
    int8_t minutes, int8_t seconds) {
  if (hours == NIXIE_BLANK_DIGIT) {
    digit_array[0] = NIXIE_BLANK_POS;
    digit_array[1] = NIXIE_BLANK_POS;
  } else {
    digit_array[0] = TENS(hours);
    digit_array[1] = ONES(hours);
  }

  if (minutes == NIXIE_BLANK_DIGIT) {
    digit_array[2] = NIXIE_BLANK_POS;
    digit_array[3] = NIXIE_BLANK_POS;
  } else {
    digit_array[2] = TENS(minutes);
    digit_array[3] = ONES(minutes);
  }

  if (seconds == NIXIE_BLANK_DIGIT) {
    digit_array[4] = NIXIE_BLANK_POS;
    digit_array[5] = NIXIE_BLANK_POS;
  } else {
    digit_array[4] = TENS(seconds);
    digit_array[5] = ONES(seconds);
  }
}
