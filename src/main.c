#include "sdkconfig.h"

#include <stdio.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_log.h>
#include <esp_timer.h>

#include <driver/adc.h>
#include <driver/gpio.h>
#include <driver/touch_pad.h>

#include <lwip/apps/sntp.h>

#include "main.h"
#include "led.h"
#include "app_wifi.h"
#include "weather.h"
#include "secret.h"
#include "displays.h"

static const char *TAG = "MAIN";

const gpio_num_t BEEPER_PIN = 33;
const adc1_channel_t LIGHT_SENSOR_CHANNEL = ADC1_GPIO35_CHANNEL;

#define NUM_TOUCH 4
const touch_pad_t TOUCH_PADS[NUM_TOUCH] = {TOUCH_PAD_NUM2, TOUCH_PAD_NUM3, TOUCH_PAD_NUM4, TOUCH_PAD_NUM5};
const uint16_t TOUCH_THRESHOLD = 800;

weather_t weather = {0};
int touched[4] = {0};

void task_display() {
    display_time();

    // Reset touch pad status
    memset(touched, 0, sizeof(touched));

    // Update LED brightness
    int val = adc1_get_raw(LIGHT_SENSOR_CHANNEL);
    led_send_all(OP_INTENSITY, 15 - val / 128 * 5); // [0, 512) => {15, 10, 5, 0}
}

void task_weather_update(void *args) {
    ESP_LOGI(TAG, "Retrieving weather data");
    get_weather(&weather);
}

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
    led_init();

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

    // Display task
    esp_timer_create_args_t display_timer_args = {
        .callback = task_display,
        .name = "display_timer",
    };
    esp_timer_handle_t display_timer;
    esp_timer_create(&display_timer_args, &display_timer);
    esp_timer_start_periodic(display_timer, 1000000L);

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
    esp_timer_create_args_t weather_timer_args = {
        .callback = task_weather_update,
        .name = "weather_timer",
    };
    esp_timer_handle_t weather_timer;
    esp_timer_create(&weather_timer_args, &weather_timer);
    esp_timer_start_periodic(weather_timer, 10 * 60 * 1000000L);
    task_weather_update(NULL);
}
