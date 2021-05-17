#pragma once

#include "gpio.h"
#include "lpc40xx.h"
#include "ssp2.h"
#include <stdio.h>

void initialize_decoder(void);

bool mp3_decoder_needs_data(void);

void spi_send_to_mp3_decoder(char data);

void MP3_decoder__sci_write(uint8_t address, uint16_t data);