#include <Arduino.h>
#include <EEPROM.h>
#include <soc/timer_group_reg.h>
#include <soc/timer_group_struct.h>

#include "arduino_debug.h"

#define EEPROM_SIZE 10

#define NUM_ELEMENTS(x) \
  ((sizeof(x) / sizeof(0 [x])) / ((size_t)(!(sizeof(x) % sizeof(0 [x])))))

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

#define NUM_NIXIE_DIGITS 10

#define LEFT_DISPLAY(nixie_digit) (nixie_digit << 4)
#define RIGHT_DISPLAY(nixie_digit) (nixie_digit)
#define DUAL_DISPLAY(left_nixie_digit, right_nixie_digit) \
  (LEFT_DISPLAY(left_nixie_digit) | RIGHT_DISPLAY(right_nixie_digit))

const uint8_t nixie_digits[] = {NIXIE_ZERO,  NIXIE_ONE,       NIXIE_TWO,
                                NIXIE_THREE, NIXIE_FOUR,      NIXIE_FIVE,
                                NIXIE_SIX,   NIXIE_SEVEN,     NIXIE_EIGHT,
                                NIXIE_NINE,  NIXIE_BLANK_CODE};

#define NIXIE_BLANK NUM_ELEMENTS(nixie_digits)

#define MILLISECOND_TO_MICROSECONDS 1000

#define DEBOUNCE_TIME_MS 200
#define DEBOUNCE_TIME_TICKS (DEBOUNCE_TIME_MS / portTICK_PERIOD_MS)
volatile TickType_t previous_tick_count = 0;

constexpr int c_clock_pin = 12;
constexpr int c_latch_pin = 14;
constexpr int c_output_enable_pin = 27;
constexpr int c_data_pin = 26;

constexpr int c_upper_left_dot = 25;

constexpr int c_rotary_encoder_switch_pin = 16;
constexpr int c_rotary_encoder_dt_pin = 17;
constexpr int c_rotary_encoder_clk_pin = 5;

constexpr int c_buzzer_pin = 18;

TaskHandle_t g_task_configure_handle = NULL;
TaskHandle_t g_task_cycle_digit_handle = NULL;

SemaphoreHandle_t g_semaphore_configure = xSemaphoreCreateBinary();

void task_cycle_digit(void* pvParameters);
void task_configure(void* pvParameters);
void smooth_transition_nixie_digit(uint8_t next_digit, uint8_t current_digit,
                                   size_t transition_time_ms);
void _smooth_transition_helper(uint8_t next_digit, uint8_t current_digit,
                               size_t transition_time_ms);
void shift_out_nixie_digit(uint8_t digit);
void rotary_encoder_switch_isr();
uint8_t get_config_value(uint8_t initial_value);
void buzzer_click();
inline void reset_watchdog_timer();

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

  xTaskCreate(task_cycle_digit, "cycle_digit", 2000, NULL, 1,
              &g_task_cycle_digit_handle);

  xTaskCreate(task_configure, "configure", 2000, NULL, 2,
              &g_task_configure_handle);
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

void smooth_transition_nixie_digit(uint8_t next_digit, uint8_t current_digit,
                                   size_t transition_time_ms) {
  _smooth_transition_helper(NIXIE_BLANK, current_digit,
                            transition_time_ms / 2.0);
  _smooth_transition_helper(next_digit, NIXIE_BLANK, transition_time_ms / 2.0);
}

void _smooth_transition_helper(uint8_t next_digit, uint8_t current_digit,
                               size_t transition_time_ms) {
  size_t multiplex_count = 100;
  double single_digit_transition_time =
      transition_time_ms / (double)multiplex_count;

  for (size_t i = 0; i < multiplex_count; ++i) {
    double current_digit_display_proportion =
        0.5 * (cos(PI * (i / (double)multiplex_count)) + 1);
    double next_digit_display_proportion = 1 - current_digit_display_proportion;

    shift_out_nixie_digit(current_digit);
    delayMicroseconds(
        (current_digit_display_proportion * single_digit_transition_time) *
        MILLISECOND_TO_MICROSECONDS);

    shift_out_nixie_digit(next_digit);
    delayMicroseconds(
        (next_digit_display_proportion * single_digit_transition_time) *
        MILLISECOND_TO_MICROSECONDS);
  }
}

void shift_out_nixie_digit(uint8_t digit) {
  digitalWrite(c_latch_pin, LOW);

  uint8_t display_digit = LEFT_DISPLAY(nixie_digits[digit]);

  shiftOut(c_data_pin, c_clock_pin, LSBFIRST, display_digit);

  digitalWrite(c_latch_pin, HIGH);
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

uint8_t get_config_value(uint8_t initial_value) {
  uint16_t state = 0;
  int counter = initial_value;

  buzzer_click();

  while (true) {
    // Feed the watchdog timer so that the MCU isn't reset
    reset_watchdog_timer();

    // Debounce filtering for the rotary encoder
    state = (state << 1) | digitalRead(c_rotary_encoder_clk_pin) | 0xe000;

    if (state == 0xf000) {
      state = 0x0000;
      if (digitalRead(c_rotary_encoder_dt_pin)) {
        counter++;
        // Maximum value 1 nixie tube can display
        if (counter > 9) {
          counter = 9;
        }
      } else {
        counter--;
        // Minimum value nixie tubes can display
        if (counter < 0) {
          counter = 0;
        }
      }

      buzzer_click();

      debug_serial_print(" -- Value: ");
      debug_serial_println(counter);
    }
    shift_out_nixie_digit(counter);

    // Poll the semaphore to see if the encoder switch was pressed
    if (xSemaphoreTake(g_semaphore_configure, 0) == pdTRUE) {
      buzzer_click();
      return counter;
    }

    // Yield to higher priority tasks, otherwise keep running
    vTaskDelay(0);
  }
}

void buzzer_click() {
  digitalWrite(c_buzzer_pin, HIGH);
  delay(1);
  digitalWrite(c_buzzer_pin, LOW);
}

inline void reset_watchdog_timer() {
  // Feed the watchdog timer so that the MCU isn't reset
  TIMERG0.wdt_wprotect = TIMG_WDT_WKEY_VALUE;
  TIMERG0.wdt_feed = 1;
  TIMERG0.wdt_wprotect = 0;
}
