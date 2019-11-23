#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include <freertos/FreeRTOS.h>

#include "displays.h"
#include "led.h"
#include "main.h"
#include "font8x8.h"
#include "font4x4.h"
#include "weather_icon.h"

display_mode_t display_mode = &display_time;

void display_time() {
    // Handle touched
    if(app_touched[0]) {
        display_mode = &display_weather;
    } else if(app_touched[1]) {
        display_mode = &display_date;
    }

    time_t now = time(NULL);
    struct tm* local = localtime(&now);

    uint8_t mat[NUM_MATS][8] = {0};
    memcpy(mat[0], font8x8_basic[local->tm_hour / 10 + '0'], 8);
    memcpy(mat[1], font8x8_basic[local->tm_hour % 10 + '0'], 8);
    memcpy(mat[2], font8x8_basic[local->tm_min / 10 + '0'], 8);
    memcpy(mat[3], font8x8_basic[local->tm_min % 10 + '0'], 8);
    led_send_matrix(mat);
}

void display_date() {
    // Handle touched
    if(app_touched[0]) {
        display_mode = &display_weather;
    } else if(app_touched[1]) {
        display_mode = &display_time;
    }

    time_t now = time(NULL);
    struct tm* local = localtime(&now);

    uint8_t mat[NUM_MATS][8] = {0};
    memcpy(mat[0], font8x8_basic[(local->tm_mon+1) / 10 + '0'], 8);
    memcpy(mat[1], font8x8_basic[(local->tm_mon+1) % 10 + '0'], 8);
    memcpy(mat[2], font8x8_basic[local->tm_mday / 10 + '0'], 8);
    memcpy(mat[3], font8x8_basic[local->tm_mday % 10 + '0'], 8);
    led_send_matrix(mat);
}

void display_weather() {
    // Handle touched
    if(app_touched[0]) {
        display_mode = &display_time;
    } if(app_touched[1]) {
        display_mode = &display_weather_gadget;
    }

    uint8_t mat[NUM_MATS][8] = {0};

    // Weather icon
    memcpy(mat[0], weather_icon(atoi(app_weather.icon)), 8);

    int temp = (int) roundf(app_weather.temp) % 100;
    if(temp > -10) { // display {-,0-9}{0-9}℃
        // First digit, either number or -
        if(temp < 0)
            memcpy(mat[1], font8x8_basic['-'], 8);
        else
            memcpy(mat[1], font8x8_basic[abs(temp) / 10 + '0'], 8);

        // Second digit
        memcpy(mat[2], font8x8_basic[abs(temp) % 10 + '0'], 8);

        // C + °
        memcpy(mat[3], font8x8_basic['C'], 8);
        mat[3][0] |= 0b10000000;
    } else { // display -00
        memcpy(mat[1], font8x8_basic['-'], 8);
        memcpy(mat[2], font8x8_basic[abs(temp) / 10 + '0'], 8);
        memcpy(mat[3], font8x8_basic[abs(temp) % 10 + '0'], 8);
    }

    led_send_matrix(mat);
}

void display_weather_gadget() {
    // Handle touched
    if(app_touched[0]) {
        display_mode = &display_time;
    } if(app_touched[1]) {
        display_mode = &display_weather;
    }

    uint8_t mat[NUM_MATS][8] = {0};

    // Icon
    memcpy(mat[0], weather_icon(atoi(app_weather.icon)), 8);

    // Humidity
    {
        int humidity = app_weather.humidity;
        if(humidity > 99) humidity = 99;
        int letters[4] = {'H', 'M', humidity / 10 + '0', humidity % 10 + '0'};
        font4x4_merge(letters, mat[1]);
    }

    // Clouds
    {
        int clouds = app_weather.clouds;
        if(clouds > 99) clouds = 99;
        int letters[4] = {'C', 'L', clouds / 10 + '0', clouds % 10 + '0'};
        font4x4_merge(letters, mat[2]);
    }

    // Wind
    {
        int wind = (int) roundf(app_weather.wind * 1.943844f);
        int letters[4] = {'K', 'T', wind / 10 + '0', wind % 10 + '0'};
        font4x4_merge(letters, mat[3]);
    }

    led_send_matrix(mat);
}

void display_alarm() {
}
