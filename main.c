#include "FreeRTOS.h"
#include "adc.h"
#include "board_io.h"
#include "common_macros.h"
#include "decoder.h"
#include "event_groups.h"
#include "ff.h"
//#include "lcd.h"
#include "lpc40xx.h"
#include "lpc_peripherals.h"
#include "periodic_scheduler.h"
#include "pwm1.h"
#include "queue.h"
#include "semphr.h"
#include "sj2_cli.h"
#include "song_list.h"
#include "task.h"
#include <stdio.h>
#include <stdlib.h>

#include "gpio_isr.h"
#include "gpio_lab.h"

// Buttons for play/pause, next/prev and volume
static SemaphoreHandle_t pp_press_indication, np_press_indication, volume_semaphore;

static void play_pause_task(void *p);

static void volume_control(void *p);

static void next_prev_button(void *p);

void prev_next_task(void *p);

#define SONG_BLOCK_SIZE 512 //*** added song block size

typedef char songname_t[16];
typedef char songdata_t[/*1*/ SONG_BLOCK_SIZE]; // Each data is 1 byte

size_t curr_song = 0;

bool playing = false;

//***QueueHandle_t Q_songname;
QueueHandle_t Q_songname;
QueueHandle_t Q_songdata;

bool song_change = false;

volatile bool pause = false;
static void read_mp3_file(songname_t name) {
  // songdata_t name = {0};
  // xQueueReceive(Q_songname, &name[0], portMAX_DELAY);
  FIL file;

  UINT num_of_bytes_read = 0;
  uint8_t data = 110;
  if (FR_OK == f_open(&file, name, FA_READ)) {
    pause = false;
    printf("Song opened: %s\n", name);

    while ((f_eof(&file) == 0) && (num_of_bytes_read != -1)) {
      // printf("***PAUSE: %d", pause);
      while (pause) {
        xSemaphoreGive(pp_press_indication);
        xSemaphoreGive(np_press_indication);
        vTaskDelay(1);
      }

      xSemaphoreGive(pp_press_indication);
      xSemaphoreGive(np_press_indication);
      xSemaphoreGive(volume_semaphore);
      songdata_t buffer = {0}; //**** Declare buffer here. This would clear buffer for each read transaction.
      if (song_change) {
        num_of_bytes_read = -1;
        song_change = false;
      }
      f_read(&file, buffer, sizeof(buffer), &num_of_bytes_read);
      /*lcd__write_char(data);
      lcd__write_instr(data);
      lcd__drive_data_pins(data);*/
      xQueueSend(Q_songdata, buffer, portMAX_DELAY);
    }

    //*** Need to close file
    printf("Read file complete\n");
    f_close(&file);

  } else {
    printf("File not opened\n");
  }
}

/// Reader tasks receives song-name over Q_songname to start reading it
/*static void mp3_reader_task(void *p) {
  printf("Mp3_reader_task entered\n");

  while (1) {
    songname_t name = {0};
    xQueueReceive(Q_songname, &name[0], portMAX_DELAY);
    printf("Received song to play: %s\n", name);
    read_mp3_file(name);
  }
}*/

static void mp3_reader_task(void *p) {
  songdata_t name = {0};
  xQueueReceive(Q_songname, &name[0], portMAX_DELAY);
  FIL file;

  UINT num_of_bytes_read = 0;
  uint8_t data = 110;
  if (FR_OK == f_open(&file, name, FA_READ)) {
    pause = false;
    printf("Song opened: %s\n", name);

    while ((f_eof(&file) == 0) && (num_of_bytes_read != -1)) {
      // printf("***PAUSE: %d", pause);
      while (pause) {
        xSemaphoreGive(pp_press_indication);
        xSemaphoreGive(np_press_indication);
        vTaskDelay(1);
      }

      xSemaphoreGive(pp_press_indication);
      xSemaphoreGive(np_press_indication);
      xSemaphoreGive(volume_semaphore);
      songdata_t buffer = {0}; //**** Declare buffer here. This would clear buffer for each read transaction.

      f_read(&file, buffer, sizeof(buffer), &num_of_bytes_read);

      if (song_change) {
        // num_of_bytes_read = -1;
        break;
        song_change = false;
      }
      xQueueSend(Q_songdata, buffer, portMAX_DELAY);
    }

    //*** Need to close file
    printf("Read file complete\n");
    f_close(&file);

  } else {
    printf("File not opened\n");
  }
}

static void mp3_decoder_send_block(songdata_t data) {

  for (size_t index = 0; index < sizeof(songdata_t); index++) { //*** Prints char one by one
    while (!mp3_decoder_needs_data()) {
      // printf("Waiting for DREQ\n");
      // vTaskDelay(1);
    }
    spi_send_to_mp3_decoder(data[index]);
    // putchar(data[index]);
  }
}

static void mp3_player_task(void *p) {
  gpio_s voldown = gpio__construct_as_input(0, 10);
  gpio_s volup = gpio__construct_as_input(0, 1);

  gpio_s bassdown = gpio__construct_as_input(2, 9);
  gpio_s bassup = gpio__construct_as_input(2, 7);

  gpio_s trebdown = gpio__construct_as_input(0, 18);
  gpio_s trebup = gpio__construct_as_input(0, 15);

  uint16_t volData = 0X7F;
  uint16_t bassData = 0;

  songdata_t songdata;

  // vol = 0xB | range 0- 0xFEFE | range each pin (left|right) 0 -0xFE
  // treble=20 is from -8 to 7. 0 is off
  // bass id from 0 to 15. 0 is off
  uint8_t vol_REG = 0xB;
  uint8_t bass_REG = 0x2;
  uint8_t volume_current = 50; //[0-100]
  uint8_t bass_current = 0;
  // uint8_t treb_current = 0;
  int8_t treb_current = 0;
  // unsigned char treb_current = 0;
  while (1) {
    if (xSemaphoreTake(volume_semaphore, portMAX_DELAY) &&
        (!gpio__get(volup) || !gpio__get(voldown) || !gpio__get(bassup) || !gpio__get(bassdown) || !gpio__get(trebup) ||
         !gpio__get(trebdown))) {
      if (!gpio__get(voldown) && (volume_current <= 98)) {
        printf("Vol down\n");
        volume_current = volume_current + 1;
      }
      if (!gpio__get(volup) && volume_current >= 2) {
        printf("Vol up\n");
        volume_current = volume_current - 1;
      }
      if (!gpio__get(bassup) && (bass_current < 13)) {
        printf("Bass up: %d\n", bass_current);
        bass_current = bass_current + 1;
      }

      if (!gpio__get(bassdown) && (bass_current > 1)) {
        printf("Bass down: %d\n", bass_current);
        bass_current = bass_current - 1;
      }

      if (!gpio__get(trebup) && (treb_current < 6)) {
        printf("Treb up: %d\n", treb_current);
        treb_current = treb_current + 1;
      }

      if (!gpio__get(trebdown) && (treb_current > -7)) {
        printf("Treb down: %d\n", treb_current);
        // treb_current = -8;
        treb_current = treb_current - 1;
      }

      bassData = treb_current & 0x0F;
      bassData = bassData << 4;
      bassData += 10 & 0xF;
      bassData = bassData << 4;
      bassData += bass_current & 0xF;
      bassData = bassData << 4;
      bassData += 10 & 0xF;

      volData = (volume_current * 0xFE) / 100; // 0- 0xFE
    }
    if (xQueueReceive(Q_songdata, &songdata[0], portMAX_DELAY)) {
      mp3_decoder_send_block(songdata);
      MP3_decoder__sci_write(vol_REG, (volData << 8) | volData);
      MP3_decoder__sci_write(bass_REG, bassData);
    }
  }
}

/*void lcd_test(void *p) {
  char string_test[] = "This is a test!";
  printf("Trying to print: %s", string_test);
  // fprintf(stderr, "Hello");
  // lcdclear_display();
  lcdwrite_string(string_test);
}*/

int main(int argc, char const *argv[]) {
  //*** lights_off();

  sj2_cli__init();
  initialize_decoder();
  // lcd__init_pins();
  // P1();

  Q_songname = xQueueCreate(1, sizeof(songname_t));
  Q_songdata = xQueueCreate(1, sizeof(songdata_t));

  pp_press_indication = xSemaphoreCreateBinary();
  np_press_indication = xSemaphoreCreateBinary();
  volume_semaphore = xSemaphoreCreateBinary();

  song_list__populate();
  for (size_t song_number = 0; song_number < song_list__get_item_count(); song_number++) {
    printf("Song %2d: %s\n", (1 + song_number), song_list__get_name_for_item(song_number));
  }
  playing = false;
  xTaskCreate(mp3_reader_task, "mp3_reader", 4096 / sizeof(void *), NULL, PRIORITY_LOW, NULL);
  xTaskCreate(mp3_player_task, "mp3_player", 4096 / sizeof(void *), NULL, PRIORITY_MEDIUM, NULL);
  xTaskCreate(play_pause_task, "play_pause", 4096 / sizeof(void *), NULL, PRIORITY_HIGH, NULL);
  xTaskCreate(prev_next_task, "prev_next", 4096 / sizeof(void *), NULL, PRIORITY_HIGH, NULL);
  // xTaskCreate(lcd_test, "lcd", 512, NULL, PRIORITY_LOW, NULL);

  vTaskStartScheduler();
  return 0;
}

void play_pause_task(void *p) {
  gpio_s pp_button = gpio__construct_as_input(2, 5);
  while (1) {
    if (xSemaphoreTake(pp_press_indication, portMAX_DELAY) && !gpio__get(pp_button)) { // Check if it pressed
      printf("Acknowledge Play/Pause\n");
      pause = !pause;
      vTaskDelay(1000);
    }
  }
}

void prev_next_task(void *p) {
  gpio_s np_button = gpio__construct_as_input(2, 2);
  TickType_t last_wake_time;

  const TickType_t sample_interval = 100 / portTICK_PERIOD_MS;
  int count = 0;
  while (1) {

    /*if (playing == false) {
      printf("Starting to play\n");
      // read_mp3_file(song_list__get_name_for_item(0));
      xQueueSend(Q_songname, song_list__get_name_for_item(0), portMAX_DELAY);
      curr_song = 0;
      playing = true;
    }*/

    if (xSemaphoreTake(np_press_indication, portMAX_DELAY) && !gpio__get(np_button)) { // Check if it pressed
      printf("Acknowledge Prev/Next\n");

      count = 0;
      while (!gpio__get(np_button))
        count++;
      if (count < 1000000) {
        printf("Next: %d\n", curr_song);
        if (curr_song < song_list__get_item_count()) {
          song_change = true;
          xQueueSend(Q_songname, song_list__get_name_for_item(++curr_song), portMAX_DELAY);
          // read_mp3_file(song_list__get_name_for_item(++curr_song));
        } else {
          // read_mp3_file(song_list__get_name_for_item(0));
          song_change = true;
          xQueueSend(Q_songname, song_list__get_name_for_item(0), portMAX_DELAY);
        }
      } else {
        printf("Prev: %d\n", curr_song);

        if (curr_song > 0) {
          //  read_mp3_file(song_list__get_name_for_item(--curr_song));
          song_change = true;
          xQueueSend(Q_songname, song_list__get_name_for_item(--curr_song), portMAX_DELAY);
        } else {
          // read_mp3_file(song_list__get_name_for_item(song_list__get_item_count()));
          song_change = true;
          xQueueSend(Q_songname, song_list__get_name_for_item(song_list__get_item_count()), portMAX_DELAY);
        }
      }
      // vTaskDelayUntil( &last_wake_time, sample_interval );
      vTaskDelay(1000);
    }
  }
}