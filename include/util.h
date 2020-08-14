#pragma once

#include <soc/timer_group_reg.h>
#include <soc/timer_group_struct.h>
#include <stddef.h>

extern const size_t c_minute_freertos;
#define MINUTE_FREERTOS c_minute_freertos

#define MILLISECOND_TO_MICROSECONDS 1000

#define NUM_ELEMENTS(x) \
  ((sizeof(x) / sizeof(0 [x])) / ((size_t)(!(sizeof(x) % sizeof(0 [x])))))

extern const int c_buzzer_pin;

void buzzer_click();

inline void reset_watchdog_timer() {
  // Feed the watchdog timer so that the MCU isn't reset
  TIMERG0.wdt_wprotect = TIMG_WDT_WKEY_VALUE;
  TIMERG0.wdt_feed = 1;
  TIMERG0.wdt_wprotect = 0;
}
