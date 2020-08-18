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

void Nixie_Display::display_time(const struct tm& time_info,
                                 bool twelve_hour_format, uint8_t nixie_dots) {
  uint8_t hours = time_info.tm_hour;
  if (twelve_hour_format) {
    hours = convert_24_hour_to_12_hour(hours);
  }

  m_digits[0] = TENS(hours);
  m_digits[1] = ONES(hours);

  m_digits[2] = TENS(time_info.tm_min);
  m_digits[3] = ONES(time_info.tm_min);

  m_digits[4] = TENS(time_info.tm_sec);
  m_digits[5] = ONES(time_info.tm_sec);

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
