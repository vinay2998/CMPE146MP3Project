#pragma once
#include "delay.h"
#include <stdbool.h>
#include <stdint.h>

void lcd__init_pins(void);

void lcd__set_gpio(void);

void lcd__two_lines_8_bit_bus(void);

void lcd__control(void);

void lcd__entry_mode(void);

void lcd__init_pins(void);

void lcd__write_data(void);

void lcd__display_cursor(void); //

void lcd__something(void); //

void lcd__function_set(void); //

void lcd__clear_display(void); //

uint8_t ascii_to_bin(char c);

void lcd__write_char(uint8_t data);

void lcd__write_instr(uint8_t data);

void lcd__drive_data_pins(uint8_t data);