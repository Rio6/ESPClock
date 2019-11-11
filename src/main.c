#include "sdkconfig.h"

#include <stdio.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_log.h>
#include <esp_timer.h>

#include <driver/adc.h>
#include <driver/gpio.h>
#include <driver/touch_pad.h>

#include <lwip/apps/sntp.h>

#include "matrix.h"
#include "app_wifi.h"
#include "weather.h"
#include "secret.h"

static const char *TAG = "MAIN";

const gpio_num_t BEEPER_PIN = 33;
const adc1_channel_t LIGHT_SENSOR_CHANNEL = ADC1_GPIO35_CHANNEL;

#define TOUCH_THRESHOLD 800
#define NUM_TOUCH 4
const touch_pad_t TOUCH_PADS[NUM_TOUCH] = {TOUCH_PAD_NUM2, TOUCH_PAD_NUM3, TOUCH_PAD_NUM4, TOUCH_PAD_NUM5};

void task_display() {
}

weather_t weather = {0};
void timer_weather_update(void *args) {
    weather_t weather
    ESP_LOGI(TAG, "Retrieving weather data");
    get_weather(&weather);
}

int touched[4] = {0};
void IRAM_ATTR intr_touched(void* args) {
    //ESP_LOGI(TAG, "Touched %d", touch_pad_get_status());
    uint16_t status = touch_pad_get_status();
    for(int i = 0; i < NUM_TOUCH; i++) {
        if((status >> TOUCH_PADS[i]) & 1) {
            touched[i] = 1;
        }
    }
    touch_pad_clear_status();
}

void app_main() {

    // Init stuff
    // LED matrix
    init_matrix();

    // Beeper
    gpio_set_direction(BEEPER_PIN, GPIO_MODE_OUTPUT);

    // Touch sensor
    touch_pad_init();
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
    touch_pad_set_trigger_mode(TOUCH_TRIGGER_BELOW);
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
    for(int i = 0; i < NUM_TOUCH; i++) {
        touch_pad_config(TOUCH_PADS[i], TOUCH_THRESHOLD);
    }
    touch_pad_filter_start(10);
    touch_pad_isr_register(intr_touched, NULL);
    touch_pad_intr_enable();

    // Light sensor
    adc1_config_width(ADC_WIDTH_BIT_9);
    adc1_config_channel_atten(LIGHT_SENSOR_CHANNEL, ADC_ATTEN_DB_11);

    // Timer
    // Weather update timer
    esp_timer_create_args_t timer_args = {
        .callback = timer_weather_update,
        .name = "weather_timer",
    };
    esp_timer_handle_t weather_timer;
    esp_timer_create(&timer_args, &weather_timer);

    // Alarm
    // TODO

    // Wifi
    uint8_t ssid[32] = WIFI_SSID;
    uint8_t pass[32] = WIFI_PASSWORD;
    app_wifi_initialize(ssid, pass);
    app_wifi_wait_connected();

    // Things that need internet

    // SNTP
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();

    // Weather
    init_weather();

    // Start things
    // Start display task TODO find out optimal size with uxTaskGetStackHighWaterMark
    //vTaskCreate(&task_display, "DISPLAY", 2048, NULL, 10, NULL);

    // Weather updater
    esp_timer_start_periodic(weather_timer, 10 * 60 * 1000000L);

    // test matrix
    while(true) {
        for(int device = 0; device < NUM_MATS; device++) {
            for(int i = 0; i < 8; i++) {
                for(int j = 0; j < 8; j++) {
                    mat_send(device, i+1, 1 << j);
                    vTaskDelay(50 / portTICK_PERIOD_MS);

                    // Touch
                    //uint16_t touch = 0;
                    //touch_pad_read(TOUCH_PAD_NUM0, &touch);
                    //if(touch < TOUCH_THRESHOLD)
                    //    gpio_set_level(BEEPER_PIN, 1);
                    //else
                    //    gpio_set_level(BEEPER_PIN, 0);

                    // Light
                    int val = adc1_get_raw(LIGHT_SENSOR_CHANNEL);
                    mat_send_all(OP_INTENSITY, 16 - val / 32); // 0~512 => 16~0
                }
                mat_send(device, i+1, 0);
            }
        }
    }
}
