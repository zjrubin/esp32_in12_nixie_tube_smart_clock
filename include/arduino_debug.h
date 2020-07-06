#pragma once

#include <stdio.h>

#ifdef ARDUINO_DEBUG
#define DEBUG_TEST 1
#else
#define DEBUG_TEST 0
#endif

#define DEBUG_OUTPUT_MAX_SIZE 100

#define debug_serial_printfln(message, ...)                                 \
  do {                                                                      \
    if (DEBUG_TEST) {                                                       \
      char debug_output_buf[DEBUG_OUTPUT_MAX_SIZE];                         \
      sprintf(debug_output_buf, "%s line:%d: " message, __func__, __LINE__, \
              __VA_ARGS__);                                                 \
      Serial.println(debug_output_buf);                                     \
    }                                                                       \
  } while (0)

#define debug_serial_println(message) \
  do {                                \
    if (DEBUG_TEST) {                 \
      Serial.println(message);        \
    }                                 \
  } while (0)

#define debug_serial_print(message) \
  do {                              \
    if (DEBUG_TEST) {               \
      Serial.print(message);        \
    }                               \
  } while (0)
