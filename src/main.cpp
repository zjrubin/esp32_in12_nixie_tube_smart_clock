#include <Arduino.h>
#include <EEPROM.h>

#include "Nixie_Display.h"
#include "arduino_debug.h"
#include "config.h"
#include "ntp.h"
#include "util.h"

#define DEBOUNCE_TIME_MS 200
#define DEBOUNCE_TIME_TICKS (DEBOUNCE_TIME_MS / portTICK_PERIOD_MS)
volatile TickType_t previous_tick_count = 0;

TaskHandle_t g_task_configure_handle = NULL;
TaskHandle_t g_task_cycle_digit_handle = NULL;

void task_display_time(void* pvParameters);
void task_cycle_digit(void* pvParameters);
void task_configure(void* pvParameters);
void task_set_time_from_ntp(void* pvParameters);
void smooth_transition_nixie_digit(uint8_t next_digit, uint8_t current_digit,
                                   size_t transition_time_ms);
void _smooth_transition_helper(uint8_t next_digit, uint8_t current_digit,
                               size_t transition_time_ms);
void shift_out_nixie_digit(uint8_t digit);
void rotary_encoder_switch_isr();

void setup() {
  // Display setup happens in the constructor of the Nixie_Display singleton
  // instance

  // Rotary encode setup
  pinMode(c_rotary_encoder_switch_pin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(c_rotary_encoder_switch_pin),
                  rotary_encoder_switch_isr, FALLING);
  pinMode(c_rotary_encoder_dt_pin, INPUT);
  pinMode(c_rotary_encoder_clk_pin, INPUT);

  // Buzzer setup
  pinMode(c_buzzer_pin, OUTPUT);

  Serial.begin(BAUD_RATE);

  if (ARDUINO_DEBUG) {
    // Wait for the serial monitor to properly attach to display output
    delay(1000);
  }

  // EEPROM setup
  setup_eeprom();

  // RTC Setup
  set_time_from_ntp();

  xTaskCreate(task_configure, "configure", 2000, NULL, 3,
              &g_task_configure_handle);

  xTaskCreate(task_set_time_from_ntp, "set_time_from_ntp", 5000, NULL, 2, NULL);

  // xTaskCreate(task_cycle_digit, "cycle_digit", 2000, NULL, 1,
  //             &g_task_cycle_digit_handle);

  xTaskCreate(task_display_time, "display_time", 4000, NULL, 0, NULL);
}

// Idle task
void loop() {}

void task_display_time(void* pvParameters) {
  for (;;) {
    struct tm time_info;
    if (!getLocalTime(&time_info)) {
      debug_serial_println("Failed to obtain time");
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      continue;
    }

    // Use the configured hour format
    uint8_t hour_format = EEPROM.read(EEPROM_HOUR_FORMAT_ADDRESS);
    Nixie_Display::get_instance().display_time(
        time_info, (hour_format == EEPROM_HOUR_FORMAT_OPTION_1));

    vTaskDelay(45 / portTICK_PERIOD_MS);
  }
}

// void task_cycle_digit(void* pvParameters) {
//   for (;;) {
//     for (size_t i = 0; i < NUM_NIXIE_DIGITS; ++i) {
//       smooth_transition_nixie_digit((i + 1) % NUM_NIXIE_DIGITS, i, 1000);
//       // shift_out_nixie_digit(i);
//       // delay(500);
//       digitalWrite(c_upper_left_dot, !digitalRead(c_upper_left_dot));
//       vTaskDelay(1);
//     }
//   }
// }

void task_configure(void* pvParameters) {
  for (;;) {
    if (xSemaphoreTake(g_semaphore_configure, portMAX_DELAY) == pdTRUE) {
      debug_serial_println("Configuration Menu:");
      while (true) {
        uint8_t test_config_value = EEPROM.read(2);
        debug_serial_printfln("Current config value: %d", test_config_value);
        test_config_value = get_config_value(2, test_config_value, 0, 99);
        debug_serial_printfln("Storing config value: %d", test_config_value);
        EEPROM.write(2, test_config_value);
        EEPROM.commit();
        break;
      }
    }
  }
}

void task_set_time_from_ntp(void* pvParameters) {
  // setup_time_from_ntp is called from setup, so immediately wait
  // for next time to configure the RTC
  for (;;) {
    vTaskDelay(15 * MINUTE_FREERTOS);

    set_time_from_ntp();
  }
}

void rotary_encoder_switch_isr() {
  TickType_t current_tick_count = xTaskGetTickCountFromISR();
  if (current_tick_count - previous_tick_count > DEBOUNCE_TIME_TICKS) {
    previous_tick_count = current_tick_count;
    debug_serial_println("rotary_encoder_switch_isr");
    xSemaphoreGiveFromISR(g_semaphore_configure, NULL);
    portYIELD_FROM_ISR();
  }
}
