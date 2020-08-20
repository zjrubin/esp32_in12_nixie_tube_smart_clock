#pragma once

#include <stddef.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define ONES(x) x % 10
#define TENS(x) (x / 10) % 10
#define HUNDREDS(x) (x / 100) % 10
#define THOUSANDS(x) (x / 1000) % 10

#define CYCLE(x) (x + 1) % 10

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

#define NIXIE_SMOOTH_TRANSITION_TIME_MS 980

#define LEFT_DISPLAY(nixie_digit) (nixie_digit << 4)
#define RIGHT_DISPLAY(nixie_digit) (nixie_digit)
#define DUAL_DISPLAY(left_nixie_digit, right_nixie_digit) \
  (LEFT_DISPLAY(left_nixie_digit) | RIGHT_DISPLAY(right_nixie_digit))

#define NIXIE_BLANK NUM_ELEMENTS(nixie_digits)

class Nixie_Display {
 public:
  // Setup function. Called once during setup
  static void setup_nixie_display();

  // Returns a reference to the only instance of model (following the Singleton
  // Pattern)
  static Nixie_Display& get_instance() {
    static Nixie_Display n;
    return n;
  }

  // Shift out the current time transitioning to the time 1 second from now
  void smooth_display_time(const struct tm& current_time,
                           bool twelve_hour_format = true,
                           uint8_t nixie_dots = NIXIE_DOTS_ALL);

  void display_time(const struct tm& time_info, bool twelve_hour_format = true,
                    uint8_t nixie_dots = NIXIE_DOTS_ALL);

  void display_config_value(uint8_t option_number, uint8_t value);

  void display_slot_machine_cycle(const struct tm& current_time,
                                  bool twelve_hour_format = true);

  // Get the current state of the dot separtors
  uint8_t get_dot_separators() const { return m_dots; }

  // Update the dot separators on the display, keeping all other digits the same
  void set_dot_separators(uint8_t nixie_dots) {
    m_dots = nixie_dots;
    show();
  }

  static SemaphoreHandle_t display_mutex;

 private:
  // Put the contents of the display onto the nixie tubes
  void show() const;

  static inline uint8_t convert_24_hour_to_12_hour(uint8_t hours) {
    hours = hours % 12;
    return hours == 0 ? 12 : hours;
  }

  Nixie_Display(){};
  ~Nixie_Display(){};

  // disallow copy/move construction or assignment
  Nixie_Display(const Nixie_Display&) = delete;
  Nixie_Display& operator=(const Nixie_Display&) = delete;
  Nixie_Display(Nixie_Display&&) = delete;
  Nixie_Display& operator=(Nixie_Display&&) = delete;

  static const uint8_t nixie_digits[];
  static const uint8_t nixie_dots[];

  static const int clock_pin;
  static const int latch_pin;
  static const int output_enable_pin;
  static const int data_pin;

  static const size_t num_display_digits = 6;
  static uint8_t m_digits[num_display_digits];
  static uint8_t m_dots;

  void smooth_display_transition(
      const uint8_t current_digits[num_display_digits],
      const uint8_t next_digits[num_display_digits], size_t transition_time_ms);

  void slot_machine_cycle_phase(const struct tm& end_time,
                                int num_cycling_digits, double phase_ms,
                                bool twelve_hour_format);

  static void set_time_in_array(uint8_t array[num_display_digits],
                                const struct tm& time_info,
                                bool twelve_hour_format);

  static void get_offset_time(struct tm* offset_time,
                              const struct tm& current_time, int time_delta);
};
