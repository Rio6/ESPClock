#pragma once

typedef struct {
    char weather[16];
    char icon[4];
    float temp;
    int humidity;
    float wind;
    int clouds;
    long sunrise;
    long sunset;
} weather_t;

esp_err_t weather_init();
esp_err_t weather_update();
weather_t *weather_get();
