#include "sdkconfig.h"

#include <stdio.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <driver/gpio.h>
#include <driver/touch_pad.h>

#include "matrix.h"
#include "app_wifi.h"
#include "weather.h"
#include "secret.h"

#define TOUCH_THRESHOLD 500

const gpio_num_t BEEPER = 33;

void app_main() {

    // LED matrix
    init_matrix();

    // Beeper
    gpio_set_direction(BEEPER, GPIO_MODE_OUTPUT);

    // Touch sensor
    touch_pad_init();
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
    for(touch_pad_t pad = TOUCH_PAD_NUM0; pad <= TOUCH_PAD_NUM4; pad++) {
        touch_pad_config(pad, 0);
    }

    // Wifi
    uint8_t ssid[32] = WIFI_SSID;
    uint8_t pass[32] = WIFI_PASSWORD;
    app_wifi_initialise(ssid, pass);
    app_wifi_wait_connected();

    // Weather
    init_weather();

    // Test weather
    weather_t weather = {0};
    get_weather(&weather);
    printf("Weather: %s %f %d\n", weather.weather, weather.temp, weather.humidity);

    // test matrix
    while(true) {
        for(int device = 0; device < NUM_MATS; device++) {
            for(int i = 0; i < 8; i++) {
                for(int j = 0; j < 8; j++) {
                    send(device, i+1, 1 << j);
                    vTaskDelay(50 / portTICK_PERIOD_MS);

                    // Touch
                    uint16_t touch = 0;
                    ESP_ERROR_CHECK(touch_pad_read(TOUCH_PAD_NUM0, &touch));
                    if(touch < TOUCH_THRESHOLD)
                        gpio_set_level(BEEPER, 1);
                    else
                        gpio_set_level(BEEPER, 0);
                }
                send(device, i+1, 0);
            }
        }
    }
}
