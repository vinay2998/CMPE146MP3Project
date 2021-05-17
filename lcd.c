#include "lcd.h"
#include "delay.h"
#include "gpio.h"

#include <stdio.h>

/**
 * Port on LCD                  |   SJTWO Port
 * ----------------------------------------------------------------
 * 4    Reg Select              |   P0.11
 * 6    Enable                  |   P0.0
 * 7    Data                    |   P0.22
 * 8    Data                    |   P0.17
 * 9    Data                    |   P0.16
 * 10   Data                    |   P2.8
 * 11   Data                    |   P2.6
 * 12   Data                    |   P2.4
 * 13   Data                    |   P2.1
 * 14   Data                    |   P1.29
 **/

static gpio_s reg_sel = {1, 23};
static gpio_s enable = {0, 0};
static gpio_s db0 = {0, 22};
static gpio_s db1 = {0, 17};
static gpio_s db2 = {0, 16};
static gpio_s db3 = {2, 8};
static gpio_s db4 = {2, 6};
static gpio_s db5 = {2, 4};
static gpio_s db6 = {2, 1};
static gpio_s db7 = {1, 29};

// static const uint8_t eight_bit_db = 0b00110000;
static const uint8_t eight_bit_db_2_lines = 0b00110000; // Not Working for some reason
static const uint8_t display_control = 0b00001110;
static const uint8_t return_home = 0b00000010;
static const uint8_t display_cursor = 0b00001100;

static const uint8_t char_increment_right = 0b00000110;
static const uint8_t return_cursor_home = 0b00000010;
static const uint8_t clear_display = 0b00000001;
static const uint8_t set_cursor_next_line = 0b00000000;

static gpio_s data_bus[] = {[0].port_number = 0, [0].pin_number = 22, [1].port_number = 0, [1].pin_number = 17,
                            [2].port_number = 0, [2].pin_number = 16, [3].port_number = 2, [3].pin_number = 8,
                            [4].port_number = 2, [4].pin_number = 6,  [5].port_number = 2, [5].pin_number = 4,
                            [6].port_number = 2, [6].pin_number = 1,  [7].port_number = 1, [7].pin_number = 29};

void lcd__init_pins(void) {
  gpio__construct_as_output(reg_sel.port_number, reg_sel.pin_number);
  gpio__construct_as_output(enable.port_number, enable.pin_number);
  gpio__construct_as_output(db0.port_number, db0.pin_number);
  gpio__construct_as_output(db1.port_number, db1.pin_number);
  gpio__construct_as_output(db2.port_number, db2.pin_number);
  gpio__construct_as_output(db3.port_number, db3.pin_number);
  gpio__construct_as_output(db4.port_number, db4.pin_number);
  gpio__construct_as_output(db5.port_number, db5.pin_number);
  gpio__construct_as_output(db6.port_number, db6.pin_number);
  gpio__construct_as_output(db7.port_number, db7.pin_number);

  lcd__function_set();
  lcd__clear_display();
  lcd__something();
  lcd__display_cursor();
}

void lcd__function_set(void) { lcd__write_instr(eight_bit_db_2_lines); }
void lcd__clear_display(void) { lcd__write_instr(clear_display); }
void lcd__something(void) { lcd__write_instr(return_home); }
void lcd__display_cursor(void) { lcd__write_instr(display_cursor); }

void lcd__control(void) { lcd__write_instr(display_control); }

void lcd__entry_mode(void) { lcd__write_instr(char_increment_right); }

static void enable_high(void) { gpio__set(enable); }

static void enable_low(void) { gpio__reset(enable); }

void lcd__write_char(uint8_t data) {
  enable_high();
  gpio__set(reg_sel);
  lcd__drive_data_pins(data);
  enable_low();
  gpio__reset(reg_sel);
}

void lcd__write_instr(uint8_t data) {
  enable_high();
  gpio__reset(reg_sel);
  lcd__drive_data_pins(data);
  enable_low();
}

void lcd__drive_data_pins(uint8_t data) {
  uint8_t level;
  for (int i = 7; i >= 0; i--) {
    level = (data & (1 << i));
    if (level) {
      gpio__set(data_bus[i]);
    } else {
      gpio__reset(data_bus[i]);
    }
  }
  delay_ms(2);
}

uint8_t ascii_to_bin(char c) {
  uint8_t compare;
  uint8_t binary_value = 0;
  for (int i = 7; i >= 0; i--) {
    compare = (c & (1 << i));
    if (compare) {
      binary_value |= (1 << i);
    } else {
      binary_value &= ~(1 << i);
    }
  }
  delay_ms(2);
  return binary_value;
}