#include "sdkconfig.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

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

// pins and channels
const gpio_num_t BEEPER_PIN = 33;
const adc1_channel_t LIGHT_SENSOR_CHANNEL = ADC1_GPIO35_CHANNEL;

// extern variables
weather_t app_weather = {0};

// display task
TaskHandle_t display_task_handle;

void task_display() {
    uint8_t touched = 0;
    uint8_t last_touched = 0;

    while(true) {
        ESP_LOGD(TAG, "display stack free: %d\n", uxTaskGetStackHighWaterMark(NULL));

        // Display
        display_mode(touched & ~last_touched); // last_touched used for debouncing
        last_touched = touched;

        // Update LED brightness
        int val = adc1_get_raw(LIGHT_SENSOR_CHANNEL);
        led_send_all(OP_INTENSITY, 15 - val / 128 * 5); // [0, 512) => {15, 10, 5, 0}

        // Find out next update time
        time_t now = time(NULL);
        int ms_till_minute = (60 - localtime(&now)->tm_sec) * 1000;

        // Wait until updated
        TickType_t last_wake = xTaskGetTickCount();
        touched = ulTaskNotifyTake(pdTRUE, ms_till_minute * portTICK_PERIOD_MS);

        // Clear debounce bits when it's been too long
        if(xTaskGetTickCount() - last_wake > 100 / portTICK_PERIOD_MS) {
            last_touched = 0;
        }
    }
}

void task_weather_update(void *args) {
    ESP_LOGI(TAG, "Updating weather");
    get_weather(&app_weather);
    xTaskNotify(display_task_handle, 0, eNoAction);
}

void intr_touched(void* args) {
    xTaskNotifyFromISR(display_task_handle, touch_pad_get_status(), eSetValueWithOverwrite, pdFALSE);
    touch_pad_clear_status();
}

void app_main() {
    // Init stuff
    // Set timezone
    setenv("TZ", TIME_ZONE, 1);
    tzset();

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
        touch_pad_config(TOUCH_PADS[i], 0);

        uint16_t value = 800;
        touch_pad_read(TOUCH_PADS[i], &value);
        touch_pad_set_thresh(TOUCH_PADS[i], value * 2 / 3);
    }
    touch_pad_filter_start(10);
    touch_pad_isr_register(intr_touched, NULL);
    touch_pad_intr_enable();

    // Light sensor
    adc1_config_width(ADC_WIDTH_BIT_9);
    adc1_config_channel_atten(LIGHT_SENSOR_CHANNEL, ADC_ATTEN_DB_11);

    // Display task TODO find out optimal size with uxTaskGetStackHighWaterMark
    xTaskCreate(&task_display, "DISPLAY", 2048, NULL, 10, &display_task_handle);

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
    esp_timer_handle_t weather_timer;
    esp_timer_create_args_t weather_timer_args = {
        .callback = task_weather_update,
        .name = "weather_timer",
    };
    esp_timer_create(&weather_timer_args, &weather_timer);
    esp_timer_start_periodic(weather_timer, 10 * 60 * 1000000L);
    task_weather_update(NULL);
}
