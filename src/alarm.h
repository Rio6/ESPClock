#pragma once

#include <time.h>

typedef struct {
    uint16_t time; // number of minute since midnight
    uint8_t days; // 0-7 bit = sun-sat day
    uint8_t enabled;

    int triggered; // not saved in NVS
} alarm_t;

esp_err_t alarm_load();
esp_err_t alarm_save();
int alarm_check(struct tm*);
void alarm_set(size_t, alarm_t);
void alarm_add(alarm_t);
void alarm_delete(size_t);
const alarm_t *alarm_get(size_t*);
