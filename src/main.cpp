#include <Arduino.h>
#include <EEPROM.h>

#include "arduino_debug.h"
#include "config.h"
#include "display.h"
#include "ntp.h"
#include "util.h"

#define DEBOUNCE_TIME_MS 200
#define DEBOUNCE_TIME_TICKS (DEBOUNCE_TIME_MS / portTICK_PERIOD_MS)
volatile TickType_t previous_tick_count = 0;

TaskHandle_t g_task_configure_handle = NULL;
TaskHandle_t g_task_cycle_digit_handle = NULL;

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
  // Nixie setup
  pinMode(c_clock_pin, OUTPUT);
  pinMode(c_latch_pin, OUTPUT);
  pinMode(c_output_enable_pin, OUTPUT);
  pinMode(c_data_pin, OUTPUT);
  pinMode(c_upper_left_dot, OUTPUT);

  digitalWrite(c_output_enable_pin, LOW);

  // Rotary encode setup
  pinMode(c_rotary_encoder_switch_pin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(c_rotary_encoder_switch_pin),
                  rotary_encoder_switch_isr, FALLING);
  pinMode(c_rotary_encoder_dt_pin, INPUT);
  pinMode(c_rotary_encoder_clk_pin, INPUT);

  // Buzzer setup
  pinMode(c_buzzer_pin, OUTPUT);

  // EEPROM setup
  EEPROM.begin(EEPROM_SIZE);

  Serial.begin(BAUD_RATE);

  // RTC Setup
  set_time_from_ntp();

  xTaskCreate(task_cycle_digit, "cycle_digit", 2000, NULL, 1,
              &g_task_cycle_digit_handle);

  xTaskCreate(task_configure, "configure", 2000, NULL, 2,
              &g_task_configure_handle);

  xTaskCreate(task_set_time_from_ntp, "set_time_from_ntp", 5000, NULL, 2, NULL);
}

// Idle task
void loop() {}

void task_cycle_digit(void* pvParameters) {
  for (;;) {
    for (size_t i = 0; i < NUM_NIXIE_DIGITS; ++i) {
      smooth_transition_nixie_digit((i + 1) % NUM_NIXIE_DIGITS, i, 1000);
      // shift_out_nixie_digit(i);
      // delay(500);
      digitalWrite(c_upper_left_dot, !digitalRead(c_upper_left_dot));
      vTaskDelay(1);
    }
  }
}

void task_configure(void* pvParameters) {
  for (;;) {
    if (xSemaphoreTake(g_semaphore_configure, portMAX_DELAY) == pdTRUE) {
      vTaskSuspend(g_task_cycle_digit_handle);
      debug_serial_println("Configuration Menu:");
      while (true) {
        uint8_t test_config_value = EEPROM.read(0);
        debug_serial_printfln("Current config value: %d", test_config_value);
        test_config_value = get_config_value(test_config_value);
        debug_serial_printfln("Storing config value: %d", test_config_value);
        EEPROM.write(0, test_config_value);
        EEPROM.commit();
        // Serial.println("HERE");
        // vTaskDelay(5000 / portTICK_PERIOD_MS);
        break;
      }
      vTaskResume(g_task_cycle_digit_handle);
    }
  }
}

void task_set_time_from_ntp(void* pvParameters) {
  // setup_time_from_ntp is called from setup, so immediately wait
  // for next time to configure the RTC
  for (;;) {
    vTaskDelay(15 * MINUTE_FREERTOS);

    set_time_from_ntp();
    // TODO: remove this for loop and put it elsewhere
    // for (int i = 0; i < 100; ++i) {
    //   print_local_time();
    //   vTaskDelay(1000 / portTICK_PERIOD_MS);
    // }
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
