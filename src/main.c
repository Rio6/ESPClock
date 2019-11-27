#include "sdkconfig.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_log.h>
#include <esp_timer.h>
#include <nvs_flash.h>

#include <driver/adc.h>
#include <driver/touch_pad.h>

#include <lwip/apps/sntp.h>

#include "main.h"
#include "led.h"
#include "app_wifi.h"
#include "weather.h"
#include "alarm.h"
#include "secret.h"
#include "displays.h"

static const char *TAG = "MAIN";
static int last_active = 0; // used for late night led shutdown

// pins and channels
const adc1_channel_t LIGHT_SENSOR_CHANNEL = ADC1_GPIO35_CHANNEL;
const touch_pad_t TOUCH_PADS[NUM_TOUCH] = {TOUCH_PAD_NUM2, TOUCH_PAD_NUM3, TOUCH_PAD_NUM4, TOUCH_PAD_NUM5};

// display task
TaskHandle_t display_task_handle = NULL;

void task_weather_update(void *args) {
    ESP_LOGI(TAG, "Updating weather");
    weather_update();
    xTaskNotify(display_task_handle, 0, eNoAction);
}

void task_brightness_update(void *args) {
    // Get light value
    int val = adc1_get_raw(LIGHT_SENSOR_CHANNEL);

    // Get current and sunset time
    time_t now = time(NULL);
    weather_t *weather = weather_get();

    // Shutdown LED when dark after sunset
    if(xTaskGetTickCount() - last_active > 5000 / portTICK_PERIOD_MS) {
        led_set_shutdown(
                   weather->sunset && weather->sunrise
                && val == 511
                && (now > weather->sunset || now < weather->sunrise));
    }

    led_send_all(OP_INTENSITY, val < 400 ? 10 : 0);
}

void task_display() {
    uint8_t touched = 0;
    uint8_t last_touched = 0;

    while(true) {

        // Check alarm and led
        int shut = 0, alarm = 0;
        if(touched) {
            // Turn on display (if it's off)
            shut = led_set_shutdown(0);
            // Stop alarm (if it's running)
            alarm = alarm_stop();
        }

        if(shut || alarm) // Don't input to display
            display_mode(0);
        else // Display normally
            display_mode(touched & ~last_touched); // last_touched used for debouncing

        last_touched = touched;

        // Get current time
        time_t now = time(NULL);
        struct tm *local = localtime(&now);

        // Trigger alarm
        if(alarm_check(local))
            alarm_start();

        // Wait until next minute or updated
        last_active = xTaskGetTickCount();
        int ticks_till_minute = (60 - local->tm_sec) * 1000 / portTICK_PERIOD_MS;
        touched = ulTaskNotifyTake(pdTRUE, ticks_till_minute);

        // Clear debounce bits when it's been too long
        if(xTaskGetTickCount() - last_active > 50 / portTICK_PERIOD_MS) {
            last_touched = 0;
        }

        ESP_LOGD(TAG, "display stack free: %d", uxTaskGetStackHighWaterMark(NULL));
    }
}

void intr_touched(void* args) {
    xTaskNotifyFromISR(display_task_handle, touch_pad_get_status(), eSetValueWithOverwrite, pdFALSE);
    touch_pad_clear_status();
}

void app_main() {
    // Set timezone
    setenv("TZ", TIME_ZONE, 1);
    tzset();

    // Setup NVS
    esp_err_t err = nvs_flash_init();
    if(err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGI(TAG, "Erasing NVS");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // LED matrix
    ESP_ERROR_CHECK(led_init());

    // Touch sensor
    ESP_ERROR_CHECK(touch_pad_init());
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
    touch_pad_set_trigger_mode(TOUCH_TRIGGER_BELOW);
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
    for(int i = 0; i < NUM_TOUCH; i++) {
        ESP_ERROR_CHECK(touch_pad_config(TOUCH_PADS[i], 0));

        uint16_t value = 800;
        touch_pad_read(TOUCH_PADS[i], &value);
        touch_pad_set_thresh(TOUCH_PADS[i], value * 2 / 3);
    }
    ESP_ERROR_CHECK(touch_pad_filter_start(10));
    ESP_ERROR_CHECK(touch_pad_isr_register(intr_touched, NULL));
    touch_pad_intr_enable();

    // Light sensor
    adc1_config_width(ADC_WIDTH_BIT_9);
    adc1_config_channel_atten(LIGHT_SENSOR_CHANNEL, ADC_ATTEN_DB_11);

    // LED brightness updater
    esp_timer_handle_t brightness_timer;
    esp_timer_create_args_t brightness_timer_args = {
        .callback = task_brightness_update,
        .name = "brightness_timer",
    };
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_timer_create(&brightness_timer_args, &brightness_timer));
    esp_timer_start_periodic(brightness_timer, 1000000L);

    // Display task
    xTaskCreate(&task_display, "DISPLAY", 2048, NULL, 10, &display_task_handle);

    // Alarm
    ESP_ERROR_CHECK(alarm_init());

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
    ESP_ERROR_CHECK_WITHOUT_ABORT(weather_init());
    esp_timer_handle_t weather_timer;
    esp_timer_create_args_t weather_timer_args = {
        .callback = task_weather_update,
        .name = "weather_timer",
    };
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_timer_create(&weather_timer_args, &weather_timer));
    esp_timer_start_periodic(weather_timer, 10 * 60 * 1000000L);
    task_weather_update(NULL);
}
