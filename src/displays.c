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

    led_send_matrix(0, font8x8_basic[local->tm_hour / 10 + '0']);
    led_send_matrix(1, font8x8_basic[local->tm_hour % 10 + '0']);
    led_send_matrix(2, font8x8_basic[local->tm_min / 10 + '0']);
    led_send_matrix(3, font8x8_basic[local->tm_min % 10 + '0']);
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

    led_send_matrix(0, font8x8_basic[(local->tm_mon+1) / 10 + '0']);
    led_send_matrix(1, font8x8_basic[(local->tm_mon+1) % 10 + '0']);
    led_send_matrix(2, font8x8_basic[local->tm_mday / 10 + '0']);
    led_send_matrix(3, font8x8_basic[local->tm_mday % 10 + '0']);
}

void display_weather() {
    // Handle touched
    if(app_touched[0]) {
        display_mode = &display_time;
    } if(app_touched[1]) {
        display_mode = &display_weather_gadget;
    }

    // Weather icon
    led_send_matrix(0, weather_icon(atoi(app_weather.icon)));

    int temp = (int) roundf(app_weather.temp) % 100;
    if(temp > -10) { // display {-,0-9}{0-9}℃
        // First digit, either number or -
        if(temp < 0)
            led_send_matrix(1, font8x8_basic['-']);
        else
            led_send_matrix(1, font8x8_basic[abs(temp) / 10 + '0']);

        // Second digit
        led_send_matrix(2, font8x8_basic[abs(temp) % 10 + '0']);

        // C + °
        uint8_t mat[8];
        memcpy(mat, font8x8_basic['C'], 8);
        mat[0] |= 0b10000000;

        led_send_matrix(3, mat);
    } else { // display -00
        led_send_matrix(1, font8x8_basic['-']);
        led_send_matrix(2, font8x8_basic[abs(temp) / 10 + '0']);
        led_send_matrix(3, font8x8_basic[abs(temp) % 10 + '0']);
    }
}

void display_weather_gadget() {
    // Handle touched
    if(app_touched[0]) {
        display_mode = &display_time;
    } if(app_touched[1]) {
        display_mode = &display_weather;
    }

    // Icon
    led_send_matrix(0, weather_icon(atoi(app_weather.icon)));

    uint8_t mat[8] = {0};

    // Humidity
    {
        int humidity = app_weather.humidity;
        if(humidity > 99) humidity = 99;
        int letters[4] = {'H', 'M', humidity / 10 + '0', humidity % 10 + '0'};
        font4x4_merge(letters, mat);
        led_send_matrix(1, mat);
    }

    // Clouds
    {
        int clouds = app_weather.clouds;
        if(clouds > 99) clouds = 99;
        int letters[4] = {'C', 'L', clouds / 10 + '0', clouds % 10 + '0'};
        font4x4_merge(letters, mat);
        led_send_matrix(2, mat);
    }

    // Wind
    {
        int wind = (int) roundf(app_weather.wind * 1.943844f);
        int letters[4] = {'K', 'T', wind / 10 + '0', wind % 10 + '0'};
        font4x4_merge(letters, mat);
        led_send_matrix(3, mat);
    }
}

void display_alarm() {
}
