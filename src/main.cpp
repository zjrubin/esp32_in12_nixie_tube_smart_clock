#include <Arduino.h>
#include <EEPROM.h>

#include "Nixie_Display.h"
#include "arduino_debug.h"
#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "ntp.h"
#include "util.h"

#define DEBOUNCE_TIME_MS 200
#define DEBOUNCE_TIME_TICKS (DEBOUNCE_TIME_MS / portTICK_PERIOD_MS)
volatile TickType_t previous_tick_count = 0;

TaskHandle_t g_task_blink_dot_separators_handle = NULL;
TaskHandle_t g_task_configure_handle = NULL;
TaskHandle_t g_task_cycle_digit_handle = NULL;
TaskHandle_t g_task_display_time_handle = NULL;

void task_display_slot_machine_cycle(void* pvParameters);
void task_display_time(void* pvParameters);
void task_cycle_digit(void* pvParameters);
void task_configure(void* pvParameters);
void task_set_time_from_ntp(void* pvParameters);
void task_blink_dot_separators(void* pvParameters);

void rotary_encoder_switch_isr();

void setup() {
  // Nixie display setup
  Nixie_Display::setup_nixie_display();

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

  xTaskCreate(task_blink_dot_separators, "blink_dot_separators", 2000, NULL, 5,
              &g_task_blink_dot_separators_handle);

  xTaskCreate(task_configure, "configure", 2000, NULL, 4,
              &g_task_configure_handle);

  xTaskCreate(task_display_slot_machine_cycle, "slot_machine_cycle", 2000, NULL,
              3, NULL);

  xTaskCreate(task_set_time_from_ntp, "set_time_from_ntp", 5000, NULL, 2, NULL);

  xTaskCreate(task_display_time, "display_time", 4000, NULL, 0,
              &g_task_display_time_handle);

  // xTaskCreate(task_cycle_digit, "cycle_digit", 2000, NULL, 1,
  //             &g_task_cycle_digit_handle);
}

// Idle task
void loop() {}

void task_display_slot_machine_cycle(void* pvParameters) {
  TickType_t previous_wake_time = xTaskGetTickCount();

  for (;;) {
    struct tm time_info;
    memset(&time_info, 0, sizeof(time_info));
    // if (!getLocalTime(&time_info)) {
    //   debug_serial_println("Failed to obtain time");
    //   vTaskDelay(1000 / portTICK_PERIOD_MS);
    //   continue;
    // }

    if (xSemaphoreTake(Nixie_Display::display_mutex, portMAX_DELAY) == pdTRUE) {
      Nixie_Display::get_instance().display_slot_machine_cycle(&time_info);
      xSemaphoreGive(Nixie_Display::display_mutex);
    }

    // vTaskDelay((30 * 1000) / portTICK_PERIOD_MS);

    uint8_t slot_machine_cycle_frequency =
        EEPROM.read(EEPROM_SLOT_MACHINE_CYCLE_FREQUENCY_ADDRESS);
    vTaskDelayUntil(&previous_wake_time,
                    slot_machine_cycle_frequency * MINUTE_FREERTOS);
  }
}

void task_display_time(void* pvParameters) {
  TickType_t previous_wake_time = xTaskGetTickCount();

  for (;;) {
    struct tm time_info;
    if (!getLocalTime(&time_info)) {
      debug_serial_println("Failed to obtain time");
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      continue;
    }

    // Use the configured hour format
    uint8_t hour_format = EEPROM.read(EEPROM_12_HOUR_FORMAT_ADDRESS);

    if (xSemaphoreTake(Nixie_Display::display_mutex, portMAX_DELAY) == pdTRUE) {
      // Nixie_Display::get_instance().display_time(time_info, hour_format);
      Nixie_Display::get_instance().smooth_display_time(time_info, hour_format);
      xSemaphoreGive(Nixie_Display::display_mutex);
    }

    // reset_watchdog_timer();
    // vTaskDelay(45 / portTICK_PERIOD_MS);
    vTaskDelayUntil(&previous_wake_time, 1000 / portTICK_PERIOD_MS);
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
      vTaskResume(g_task_blink_dot_separators_handle);
      handle_configuration();
      vTaskSuspend(g_task_blink_dot_separators_handle);
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

void task_blink_dot_separators(void* pvParameters) {
  // Suspend by default. The configuration task will resume this task
  vTaskSuspend(NULL);

  for (;;) {
    TickType_t previous_wake_time = xTaskGetTickCount();

    if (xSemaphoreTake(Nixie_Display::display_mutex, portMAX_DELAY) == pdTRUE) {
      uint8_t dot_separators =
          Nixie_Display::get_instance().get_dot_separators();
      dot_separators =
          (dot_separators == NIXIE_DOTS_ALL) ? NIXIE_DOTS_NONE : NIXIE_DOTS_ALL;
      Nixie_Display::get_instance().set_dot_separators(dot_separators);

      xSemaphoreGive(Nixie_Display::display_mutex);
    }

    vTaskDelayUntil(&previous_wake_time, 1000 / portTICK_PERIOD_MS);
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
