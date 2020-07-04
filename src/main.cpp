#include <Arduino.h>

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
#define NIXIE_BLANK 0b1111

#define LEFT_DISPLAY(nixie_digit) (nixie_digit << 4)
#define RIGHT_DISPLAY(nixie_digit) (nixie_digit)
#define DUAL_DISPLAY(left_nixie_digit, right_nixie_digit) \
    (LEFT_DISPLAY(left_nixie_digit) | RIGHT_DISPLAY(right_nixie_digit))

const uint8_t nixie_digits[] = {NIXIE_ZERO, NIXIE_ONE, NIXIE_TWO,
                                NIXIE_THREE,
                                NIXIE_FOUR, NIXIE_FIVE, NIXIE_SIX,
                                NIXIE_SEVEN, NIXIE_EIGHT, NIXIE_NINE};

constexpr int c_clock_pin = 12;
constexpr int c_latch_pin = 14;
constexpr int c_output_enable_pin = 27;
constexpr int c_data_pin = 26;

constexpr int c_upper_left_dot = 25;

void shift_out_nixie_digit(uint8_t digit);

void setup()
{
    pinMode(c_clock_pin, OUTPUT);
    pinMode(c_latch_pin, OUTPUT);
    pinMode(c_output_enable_pin, OUTPUT);
    pinMode(c_data_pin, OUTPUT);
    pinMode(c_upper_left_dot, OUTPUT);

    digitalWrite(c_output_enable_pin, LOW);
}

void loop()
{
    for (int i = 0; i < NUM_ELEMENTS(nixie_digits); ++i)
    {
        shift_out_nixie_digit(i);
        delay(500);
        digitalWrite(c_upper_left_dot, !digitalRead(c_upper_left_dot));
    }
}

void shift_out_nixie_digit(uint8_t digit)
{
    digitalWrite(c_latch_pin, LOW);

    uint8_t display_digit = LEFT_DISPLAY(nixie_digits[digit]);

    shiftOut(c_data_pin, c_clock_pin, LSBFIRST, display_digit);

    digitalWrite(c_latch_pin, HIGH);
}
