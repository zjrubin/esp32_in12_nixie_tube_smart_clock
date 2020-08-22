
#include "special_modes.h"

#include <stdint.h>

#include "Nixie_Display.h"
#include "arduino_debug.h"
#include "config.h"
#include "tasks.h"
#include "util.h"

void timer_mode() {
  //  Set the display to all zeros
  Nixie_Display::get_instance().display_value(0, 0, 0);

  static const uint8_t num_timer_sections = 3;

  // Configure the timer duration
  vTaskResume(g_task_blink_dot_separators_handle);
  uint8_t timer_duration[num_timer_sections];
  memset(timer_duration, 0, sizeof(timer_duration));
  for (size_t i = 0; i < num_timer_sections; ++i) {
    timer_duration[i] =
        get_config_value(i, 0, 0, 99, &Nixie_Display::display_timer_select);

    Nixie_Display::get_instance().display_value(
        timer_duration[0], timer_duration[1], timer_duration[2]);
  }
  vTaskSuspend(g_task_blink_dot_separators_handle);

  // Form the struct to manage the timer countdown
  // Initialize unused timer struct members to known safe values
  struct tm timer;
  timer.tm_yday = 233;
  timer.tm_wday = 5;
  timer.tm_year = 120;
  timer.tm_mon = 7;
  timer.tm_mday = 21;

  // Initialize the timer struct members that are actually displayed
  timer.tm_hour = timer_duration[0];
  timer.tm_min = timer_duration[1];
  timer.tm_sec = timer_duration[2];

  debug_serial_printf(
      "timer:\n\ttm_isdst: %d\n\ttm_yday: %d\n\ttm_wday: %d\n\ttm_year: "
      "%d\n\ttm_mon: %d\n\ttm_mday: %d\n\ttm_hour: %d\n\ttm_min: %d\n\ttm_sec: "
      "%d\n",
      timer.tm_isdst, timer.tm_yday, timer.tm_wday, timer.tm_year, timer.tm_mon,
      timer.tm_mday, timer.tm_hour, timer.tm_min, timer.tm_sec);

  // Countdown the timer
  while (timer.tm_hour != 0 || timer.tm_min != 0 || timer.tm_sec != 0) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    // Count down a second and normalize the time struct
    Nixie_Display::get_offset_time(&timer, timer, -1);

    Nixie_Display::get_instance().display_value(timer.tm_hour, timer.tm_min,
                                                timer.tm_sec);

    // Exit timer mode if the encoder switch was pressed
    if (xSemaphoreTake(g_semaphore_configure, 0) == pdTRUE) {
      buzzer_click();
      return;
    }
  }

  // Countdown is finished. Sound the buzzer and exit
  const size_t timer_buzzer_duration_ms = 10 * 1000;
  const size_t num_buzzer_cycles = 150;
  double buzzer_cycle_delay_ms =
      timer_buzzer_duration_ms / (double)num_buzzer_cycles;

  for (size_t i = 0; i < num_buzzer_cycles; ++i) {
    buzzer_click();
    vTaskDelay(buzzer_cycle_delay_ms / portTICK_PERIOD_MS);
  }
}
