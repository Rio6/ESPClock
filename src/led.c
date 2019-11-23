#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_log.h>
#include <driver/touch_pad.h>

#include "led.h"

#define MOSI 23
#define CLK  19
#define CS   22

#define BUFF_SIZE NUM_MATS * 2

static const char *TAG = "LED";

void led_init() {
    // setup SPI pins
    ESP_ERROR_CHECK(gpio_set_direction(MOSI, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_direction(CLK, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_direction(CS, GPIO_MODE_OUTPUT));

    // Init MAX7912
    led_send_all(OP_SCANLIMIT, 0x7);
    led_send_all(OP_INTENSITY, 0x7);
    led_send_all(OP_SHUTDOWN, 0x1);
    led_send_all(OP_DECODEMODE, 0);
    led_send_all(OP_DISPLAYTEST, 0);

    // Clear the matrix
    for(int i = OP_DIGIT0; i <= OP_DIGIT7; i++) {
        led_send_all(i, 0);
    }
}

static void send_buff(uint8_t buff[BUFF_SIZE]) {

    // Create a semaphore - only one task can send buff at a time
    static volatile SemaphoreHandle_t sema = NULL;
    if(!sema) {
        sema = xSemaphoreCreateMutex();
        if(!sema) {
            ESP_LOGE(TAG, "cannot create semaphore");
            abort();
        }
    }

    xSemaphoreTake(sema, portMAX_DELAY);

    // Start sending
    gpio_set_level(CS, 0);
    vTaskDelay(1);

    for(int i = 0; i < BUFF_SIZE; i++) {
        for(int j = 7; j >= 0; j--) {
            gpio_set_level(CLK, 0);
            gpio_set_level(MOSI, (buff[i] >> j) & 0x01);
            gpio_set_level(CLK, 1);
        }
    }

    // Finish sending
    gpio_set_level(CS, 1);
    vTaskDelay(1);

    xSemaphoreGive(sema);
}

void led_send(int channel, uint8_t op, uint8_t data) {
    uint8_t buff[BUFF_SIZE] = {0};

    buff[channel*2] = op;
    buff[channel*2+1] = data;

    send_buff(buff);
}

void led_send_all(uint8_t op, uint8_t data) {
    uint8_t buff[BUFF_SIZE];

    for(int i = 0; i < BUFF_SIZE; i+=2) {
        buff[i] = op;
        buff[i+1] = data;
    }

    send_buff(buff);
}

void led_send_matrix(const uint8_t mat[NUM_MATS][8]) {
    for(int i = 0; i < 8; i++) {
        uint8_t buff[BUFF_SIZE];
        for(int ch = 0; ch < NUM_MATS; ch++) {
            buff[ch*2] = OP_DIGIT0 + i;
            buff[ch*2+1] = mat[ch][i];
        }
        send_buff(buff);
    }
}
