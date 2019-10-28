#include "sdkconfig.h"

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"

#define OP_NOOP        0x0
#define OP_DIGIT0      0x1
#define OP_DIGIT1      0x2
#define OP_DIGIT2      0x3
#define OP_DIGIT3      0x4
#define OP_DIGIT4      0x5
#define OP_DIGIT5      0x6
#define OP_DIGIT6      0x7
#define OP_DIGIT7      0x8
#define OP_DECODEMODE  0x9
#define OP_INTENSITY   0xa
#define OP_SCANLIMIT   0Xb
#define OP_SHUTDOWN    0xc
#define OP_DISPLAYTEST 0xf

#define MOSI 23
#define CLK  19
#define CS   22

#define NUM_MATS 4
#define BUFF_SIZE NUM_MATS * 2

const gpio_num_t BEEPER = 33;

//spi_device_handle_t spi;
//uint8_t* buff;
uint8_t buff[BUFF_SIZE];

void send_buff(uint8_t buff[BUFF_SIZE]) {
    if(CONFIG_LOG_DEFAULT_LEVEL >= ESP_LOG_DEBUG) {
        for(int i = 0; i < BUFF_SIZE; i++) printf("%02x ", buff[i]);
        printf("\n");
    }

    gpio_set_level(CS, 0);
    vTaskDelay(1);

    for(int i = 0; i < BUFF_SIZE; i++) {
        for(int j = 7; j >= 0; j--) {
            gpio_set_level(CLK, 0);
            gpio_set_level(MOSI, (buff[i] >> j) & 0x01);
            gpio_set_level(CLK, 1);
        }
    }

    gpio_set_level(CS, 1);
    vTaskDelay(1);
}

void send(int channel, uint8_t op, uint8_t data) {
    for(int i = 0; i < BUFF_SIZE; i+=2) {
        buff[i] = OP_NOOP;
        buff[i+1] = 0;
    }

    buff[channel*2] = op;
    buff[channel*2+1] = data;

    send_buff(buff);
}

void send_all(uint8_t op, uint8_t data) {
    for(int i = 0; i < BUFF_SIZE; i+=2) {
        buff[i] = op;
        buff[i+1] = data;
    }

    send_buff(buff);
}

void app_main() {

    // setup SPI pins
    gpio_set_direction(MOSI, GPIO_MODE_OUTPUT);
    gpio_set_direction(CLK, GPIO_MODE_OUTPUT);
    gpio_set_direction(CS, GPIO_MODE_OUTPUT);

    // Init MAX7912
    send_all(OP_SCANLIMIT, 0x7);
    send_all(OP_INTENSITY, 0x7);
    send_all(OP_SHUTDOWN, 0x1);
    send_all(OP_DECODEMODE, 0);
    send_all(OP_DISPLAYTEST, 0);

    // Beeper
    gpio_set_direction(BEEPER, GPIO_MODE_OUTPUT);

    // test matrix
    while(true) {
        for(int device = 0; device < NUM_MATS; device++) {
            for(int i = 0; i < 8; i++) {
                for(int j = 0; j < 8; j++) {
                    send(device, i+1, 1 << j);
                    vTaskDelay(50 / portTICK_PERIOD_MS);
                }
                send(device, i+1, 0);
            }

            // Beep
            gpio_set_level(BEEPER, 1);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            gpio_set_level(BEEPER, 0);
        }
    }

}
