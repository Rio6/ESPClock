#include <string.h>
#include <time.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_log.h>
#include <esp_err.h>
#include <nvs_flash.h>
#include <driver/gpio.h>

#include "alarm.h"
#include "error.h"

static const char *TAG = "alarm";
static const char *ALARM_NS = "alarm";
static const char *ALARM_KEY = "alarms";
static const gpio_num_t BEEPER_PIN = 33;

static alarm_t *alarms = NULL;
static size_t alarms_len = 0;
static size_t alarms_len_max = 0;

static TaskHandle_t alarm_task_handle = NULL;

/*
 * 32 bit to represent an alarm
 * 00000000         0  00000000     0000000000000000
 *   unused | enabled | sftwtms |  minutes since midnight (unsigned)
 */
static uint32_t encode_alarm(const alarm_t *alarm) {
    return (uint32_t) alarm->enabled << 24
        | (uint32_t) alarm->days << 16
        | alarm->time;
}

static void decode_alarm(uint32_t code, alarm_t *alarm) {
    alarm->time = code & 0xFFFF;
    alarm->days = code >> 16 & 0xFF;
    alarm->enabled = code >> 24 & 0x1;
}

// (Re)allocate memory for alarms
static esp_err_t realloc_alarms(size_t len) {

    // Just change length when there's space already
    if(len <= alarms_len_max) {
        alarms_len = len;
        return ESP_OK;
    }

    size_t size = len * sizeof(alarm_t);

    // Allocate memory
    void *allocated = NULL;
    if(alarms) {
        allocated = realloc(alarms, size);
    } else {
        allocated = malloc(size);
    }

    // Error check and update alarm length
    if(!allocated) {
        ESP_LOGE(TAG, "Cannot allocate memory for alarms");
        return ESP_ERR_NO_MEM;
    } else {
        alarms = allocated;
        alarms_len = alarms_len_max = len;
    }

    return ESP_OK;
}

static void alarm_task() {
    for(int i = 0; i < 10; i++) {
        for(int j = 0; j < 4; j++) {
            gpio_set_level(BEEPER_PIN, 1);
            vTaskDelay(63 / portTICK_PERIOD_MS);
            gpio_set_level(BEEPER_PIN, 0);
            vTaskDelay(63 / portTICK_PERIOD_MS);
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    ESP_LOGD(TAG, "alarm stack free: %d", uxTaskGetStackHighWaterMark(NULL));
    vTaskDelete(NULL);
}

esp_err_t alarm_init() {
    esp_err_t err = ESP_OK;

    // Init beeper
    gpio_set_direction(BEEPER_PIN, GPIO_MODE_OUTPUT);

    // Create NVS handle
    nvs_handle handle;
    if((err = nvs_open(ALARM_NS, NVS_READWRITE, &handle)) == ESP_OK) {

        // Get stored size
        size_t size = 0;
        if((err = nvs_get_blob(handle, ALARM_KEY, NULL, &size)) == ESP_OK) {

            // Load alarm data
            size_t len = size / sizeof(uint32_t);
            uint32_t buff[len];
            nvs_get_blob(handle, ALARM_KEY, buff, &size); // Shouldn't fail if the last one didn't

            // (Re)allocate memory for alarm
            if((err = realloc_alarms(len)) == ESP_OK) {

                // Convert uint32_t to alarm_t and insert into alarms
                for(int i = 0; i < len; i++) {
                    decode_alarm(buff[i], &alarms[i]);
                }
            }
        }

        // Close NVS handle
        nvs_close(handle);
    }

    return err;
}

esp_err_t alarm_save() {
    esp_err_t err = ESP_OK;

    // Create NVS handle
    nvs_handle handle;
    if((err = nvs_open(ALARM_NS, NVS_READWRITE, &handle)) == ESP_OK) {

        // Encode alarms to uint32_t
        uint32_t buff[alarms_len];
        for(int i = 0; i < alarms_len; i++) {
            buff[i] = encode_alarm(&alarms[i]);
        }

        // Save into NVS
        if((err = nvs_set_blob(handle, ALARM_KEY, buff, alarms_len * sizeof(uint32_t))) == ESP_OK) {
            // Commit NVS
            err = nvs_commit(handle);
            ESP_LOGI(TAG, "Alarm saved");
        }

        // Close NVS handle
        nvs_close(handle);
    }

    return err;
}

void alarm_set(size_t index, alarm_t alarm) {
    if(index >= alarms_len) return;
    alarms[index] = alarm;
    ESP_ERROR_CHECK_WITHOUT_ABORT(alarm_save());
}

void alarm_add(alarm_t alarm) {
    size_t last = alarms_len;
    if(realloc_alarms(alarms_len+1) == ESP_OK) {
        alarms[last] = alarm;
    } else return;

    ESP_ERROR_CHECK_WITHOUT_ABORT(alarm_save());
}

void alarm_delete(size_t index) {
    if(index+1 < alarms_len) {
        memmove(&alarms[index], &alarms[index+1], alarms_len-index);
    }
    realloc_alarms(alarms_len-1);
    ESP_ERROR_CHECK_WITHOUT_ABORT(alarm_save());
}

const alarm_t *alarm_get(size_t *len) {
    *len = alarms_len;
    return alarms;
}

int alarm_start() {
    if(alarm_task_handle) return 0;
    xTaskCreate(&alarm_task, "ALARM", 1024, NULL, 11, &alarm_task_handle);
    return 1;
}

int alarm_stop() {
    if(!alarm_task_handle) return 0;
    vTaskDelete(alarm_task_handle);
    alarm_task_handle = NULL;
    gpio_set_level(BEEPER_PIN, 0);
    return 1;
}

int alarm_check(struct tm *local_time) {
    uint8_t today = 1 << local_time->tm_wday;
    uint16_t min_now = local_time->tm_min + local_time->tm_hour * 60;

    for(int i = 0; i < alarms_len; i++) {
        if(alarms[i].enabled
            && alarms[i].days & today
            && alarms[i].time == min_now) {

            if(!alarms[i].triggered) {
                alarms[i].triggered = 1;
                return 1;
            }
        } else alarms[i].triggered = 0;
    }
    return 0;
}
