#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include <freertos/FreeRTOS.h>

#include "displays.h"
#include "led.h"
#include "main.h"
#include "secret.h"
#include "font8x8.h"
#include "weather_icon.h"

display_mode_t display_mode = display_weather;

void display_time() {
    time_t now = time(NULL);
    struct tm* local = localtime(&now);

    if(local->tm_hour + TIME_ZONE < 0)
        local->tm_hour += 24;
    else if(local->tm_hour + TIME_ZONE >= 24)
        local->tm_hour -= 24;
    local->tm_hour += TIME_ZONE;

    led_send_matrix(0, font8x8_basic[local->tm_hour / 10 + '0']);
    led_send_matrix(1, font8x8_basic[local->tm_hour % 10 + '0']);
    led_send_matrix(2, font8x8_basic[local->tm_min / 10 + '0']);
    led_send_matrix(3, font8x8_basic[local->tm_min % 10 + '0']);
}

void display_date() {
}

void display_weather() {
    led_send_matrix(0, weather_icon(atoi(app_weather.icon)));

    int temp = (int) roundf(app_weather.temp);
    led_send_matrix(1, font8x8_basic[temp / 10 + '0']);
    led_send_matrix(2, font8x8_basic[temp % 10 + '0']);

    // Add degree to C
    uint8_t mat[8];
    memcpy(mat, font8x8_basic['C'], 8);
    mat[0] |= 0b10000000;

    led_send_matrix(3, mat);
}

void display_alarm() {
}
