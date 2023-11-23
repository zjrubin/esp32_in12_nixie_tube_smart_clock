#include <Arduino.h>
#include <EEPROM.h>

#include "Nixie_Display.h"
#include "arduino_debug.h"
#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "ntp.h"
#include "special_modes.h"
#include "tasks.h"
#include "util.h"
#include "weather.h"

#define DEBOUNCE_TIME_MS 200
#define DEBOUNCE_TIME_TICKS (DEBOUNCE_TIME_MS / portTICK_PERIOD_MS)
volatile TickType_t previous_tick_count = 0;

TaskHandle_t g_task_special_modes_handle = NULL;
TaskHandle_t g_task_configure_handle = NULL;
TaskHandle_t g_task_blink_dot_separators_handle = NULL;
TaskHandle_t g_task_display_time_handle = NULL;
TaskHandle_t g_task_display_date_handle = NULL;
TaskHandle_t g_task_display_local_temperature_handle = NULL;

void task_display_slot_machine_cycle(void* pvParameters);
void task_display_time(void* pvParameters);
void task_display_date(void* pvParameters);
void task_display_local_temperature(void* pvParameters);
void task_cycle_digit(void* pvParameters);
void task_configure(void* pvParameters);
void task_special_modes(void* pvParameters);
void task_set_time_from_ntp(void* pvParameters);
void task_blink_dot_separators(void* pvParameters);

void rotary_encoder_switch_isr();

void setup() {
  // Nixie display setup
  Nixie_Display::setup_nixie_display();

  // Zero the display
  Nixie_Display::get_instance().display_value(0, 0, 0, NIXIE_DOTS_ALL);

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

  xTaskCreate(task_blink_dot_separators, "blink_dot_separators", 2000, NULL, 80,
              &g_task_blink_dot_separators_handle);

  xTaskCreate(task_configure, "configure", 2000, NULL, 70,
              &g_task_configure_handle);

  xTaskCreate(task_special_modes, "special_modes", 2000, NULL, 60,
              &g_task_special_modes_handle);

  xTaskCreate(task_display_slot_machine_cycle, "slot_machine_cycle", 2000, NULL,
              50, NULL);

  xTaskCreate(task_set_time_from_ntp, "set_time_from_ntp", 5000, NULL, 40,
              NULL);

  xTaskCreate(task_display_date, "display_date", 2000, NULL, 30,
              &g_task_display_date_handle);

  xTaskCreate(task_display_local_temperature, "display_local_temperature",
              10000, NULL, 20, &g_task_display_local_temperature_handle);

  xTaskCreate(task_display_time, "display_time", 4000, NULL, 10,
              &g_task_display_time_handle);
}

// Idle task
void loop() {}

void task_display_slot_machine_cycle(void* pvParameters) {
  for (;;) {
    TickType_t previous_wake_time;
    struct tm time_info;

    if (xSemaphoreTake(Nixie_Display::display_mutex, portMAX_DELAY) == pdTRUE) {
      previous_wake_time = xTaskGetTickCount();

      if (!getLocalTime(&time_info)) {
        debug_serial_println("Failed to obtain time");
        xSemaphoreGive(Nixie_Display::display_mutex);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        continue;
      }

      // Use the configured hour format
      uint8_t hour_format = EEPROM.read(EEPROM_12_HOUR_FORMAT_ADDRESS);
      Nixie_Display::get_instance().display_slot_machine_cycle(time_info,
                                                               hour_format);
      xSemaphoreGive(Nixie_Display::display_mutex);
    }

    // vTaskDelay((30 * 1000) / portTICK_PERIOD_MS);

    uint8_t slot_machine_cycle_frequency =
        EEPROM.read(EEPROM_SLOT_MACHINE_CYCLE_FREQUENCY_ADDRESS);

    TickType_t delay_length =
        (time_info.tm_hour == MANDATORY_CATHODE_POISONING_PREVENTION_HOUR)
            ? (30 * 1000 / portTICK_PERIOD_MS)
            : (slot_machine_cycle_frequency * MINUTE_FREERTOS);

    vTaskDelayUntil(&previous_wake_time, delay_length);
  }
}

void task_display_time(void* pvParameters) {
  for (;;) {
    TickType_t previous_wake_time;

    if (xSemaphoreTake(Nixie_Display::display_mutex, portMAX_DELAY) == pdTRUE) {
      // vTaskDelayUntil has an odd quirk in that it if the previous wake up
      // time is initialized outside of the for(;;) loop and the task misses
      // several instances where it should have woken up, FreeRTOS it will
      // execute the task several times in a row to "catch up" for the times it
      // missed its wakeups. This behavior can be suppressed by initializing the
      // previous wake up time within the for(;;) loop on each iteration, as is
      // done here.
      previous_wake_time = xTaskGetTickCount();

      struct tm time_info;
      if (!getLocalTime(&time_info)) {
        debug_serial_println("Failed to obtain time");
        xSemaphoreGive(Nixie_Display::display_mutex);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        continue;
      }

      // Use the configured hour format
      uint8_t hour_format = EEPROM.read(EEPROM_12_HOUR_FORMAT_ADDRESS);

      // Nixie_Display::get_instance().display_time(time_info, hour_format);
      Nixie_Display::get_instance().smooth_display_time(time_info, hour_format);
      xSemaphoreGive(Nixie_Display::display_mutex);
    }

    // vTaskDelay(45 / portTICK_PERIOD_MS);
    reset_watchdog_timer();
    vTaskDelayUntil(&previous_wake_time, 1000 / portTICK_PERIOD_MS);
  }
}

void task_display_date(void* pvParameters) {
  // Upon startup or reset of the clock, the slot machine transition into
  // the time display should be seen. To prevent this task from preempting
  // those task, sleep this task when it first starts
  vTaskDelay(15 * 1000 / portTICK_PERIOD_MS);

  // Since this task can be suspended during configuration if a value of zero
  // is entered for EEPROM_DATE_DISPLAY_FREQUENCY, we need to make sure
  // this task suspends itself if zero is set. Otherwise, we will be calling
  // vTaskDelayUntil with a 0 tick delay
  if (!EEPROM.read(EEPROM_DATE_DISPLAY_FREQUENCY_ADDRESS)) {
    vTaskSuspend(NULL);
  }

  for (;;) {
    TickType_t previous_wake_time;
    if (xSemaphoreTake(Nixie_Display::display_mutex, portMAX_DELAY) == pdTRUE) {
      previous_wake_time = xTaskGetTickCount();

      struct tm time_info;
      if (!getLocalTime(&time_info)) {
        debug_serial_println("Failed to obtain time");
        xSemaphoreGive(Nixie_Display::display_mutex);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        continue;
      }

      Nixie_Display::get_instance().display_date(time_info);
      vTaskDelay(8 * 1000 / portTICK_PERIOD_MS);  // Keep date on display
      xSemaphoreGive(Nixie_Display::display_mutex);
    }

    uint8_t date_display_frequency =
        EEPROM.read(EEPROM_DATE_DISPLAY_FREQUENCY_ADDRESS);

    vTaskDelayUntil(&previous_wake_time,
                    date_display_frequency * MINUTE_FREERTOS);
  }
}

void task_display_local_temperature(void* pvParameters) {
  for (;;) {
    double temperature;
    if (!get_local_temperature(&temperature)) {
      vTaskDelay(10 * MINUTE_FREERTOS);
      continue;
    }

    size_t rounded_temperature = static_cast<size_t>(temperature);

    // Shift the decimals 2 places to the left
    double decimal_f = temperature - rounded_temperature;
    uint8_t decimal = static_cast<uint8_t>(decimal_f * 100);

    uint8_t tens_and_ones =
        (10 * ((uint8_t)(rounded_temperature / (double)10) % 10)) +
        (rounded_temperature % 10);

    uint8_t thousands_and_hundreds =
        (10 * ((uint8_t)(rounded_temperature / (double)1000)) % 10) +
        ((uint8_t)(rounded_temperature / (double)100) % 10);

    debug_serial_printf(
        "thousands_and_hundreds: %hhu\ttens_and_ones: %hhu\tdecimal: %hhu\n",
        thousands_and_hundreds, tens_and_ones, decimal);

    if (xSemaphoreTake(Nixie_Display::display_mutex, portMAX_DELAY) == pdTRUE) {
      Nixie_Display::get_instance().smooth_display_value(
          500,
          thousands_and_hundreds == 0 ? NIXIE_BLANK_DIGIT
                                      : thousands_and_hundreds,
          tens_and_ones, decimal, NIXIE_DOTS_BOTTOM_RIGHT, true);

      vTaskDelay(20 * 1000 / portTICK_PERIOD_MS);

      xSemaphoreGive(Nixie_Display::display_mutex);
    }

    vTaskDelay(5 * MINUTE_FREERTOS);
  }
}

void task_configure(void* pvParameters) {
  for (;;) {
    if (xSemaphoreTake(g_semaphore_configure, portMAX_DELAY) == pdTRUE) {
      // Do not allow any other tasks to output to the display while
      // configuration is taking place
      xSemaphoreTake(Nixie_Display::display_mutex, portMAX_DELAY);

      vTaskResume(g_task_blink_dot_separators_handle);
      handle_configuration();
      vTaskSuspend(g_task_blink_dot_separators_handle);

      xSemaphoreGive(Nixie_Display::display_mutex);
    }
  }
}

void task_special_modes(void* pvParameters) {
  for (;;) {
    vTaskSuspend(NULL);

    if (xSemaphoreTake(Nixie_Display::display_mutex, portMAX_DELAY) == pdTRUE) {
      // We need to suspend the configuration task so that it doesn't
      // interfere with g_semaphore_configure while one of the special modes
      // is receiving input
      vTaskSuspend(g_task_configure_handle);

      uint8_t eeprom_special_mode = EEPROM.read(EEPROM_SPECIAL_MODES_ADDRESS);

      // Set the config value back to zero to resume normal clock operation
      // Once the special mode is done
      EEPROM.write(EEPROM_SPECIAL_MODES_ADDRESS, 0);
      EEPROM.commit();

      switch (eeprom_special_mode) {
        case 1:
          timer_mode();
          break;

        default:
          break;
      }

      vTaskResume(g_task_configure_handle);
      xSemaphoreGive(Nixie_Display::display_mutex);
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

  // Since this function is currently only resumed from the configuration task,
  // it is unnecessary for this function to lock the display mutex.
  // The configuration task already holds the display mutex and the
  // configuration task does not modify the dot separators; It lets this task
  // handle that...
  for (;;) {
    TickType_t previous_wake_time = xTaskGetTickCount();

    uint8_t dot_separators = Nixie_Display::get_instance().get_dot_separators();
    dot_separators =
        (dot_separators == NIXIE_DOTS_ALL) ? NIXIE_DOTS_NONE : NIXIE_DOTS_ALL;
    Nixie_Display::get_instance().set_dot_separators(dot_separators);

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
