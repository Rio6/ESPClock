#include <stdio.h>
#include <string.h>
#include <time.h>

#include <freertos/FreeRTOS.h>

#include "displays.h"
#include "led.h"
#include "secret.h"

void display_time() {
    time_t now = time(NULL);
    struct tm* local = localtime(&now);

    if(local->tm_hour + TIME_ZONE < 0)
        local->tm_hour += 24;
    else if(local->tm_hour + TIME_ZONE >= 24)
        local->tm_hour -= 24;
    local->tm_hour += TIME_ZONE;

    uint8_t mat[8] = {0};

    led_set_matrix(local->tm_hour / 10 + '0', mat);
    led_send_matrix(0, mat);

    memset(mat, 0, sizeof(mat));
    led_set_matrix(local->tm_hour % 10 + '0', mat);
    led_send_matrix(1, mat);

    memset(mat, 0, sizeof(mat));
    led_set_matrix(local->tm_min / 10 + '0', mat);
    led_send_matrix(2, mat);

    memset(mat, 0, sizeof(mat));
    led_set_matrix(local->tm_min % 10 + '0', mat);
    led_send_matrix(3, mat);
}

void display_weather() {
}

void display_alarm() {
}
