#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include <freertos/FreeRTOS.h>

#include "displays.h"
#include "led.h"
#include "main.h"
#include "weather.h"
#include "alarm.h"
#include "font8x8.h"
#include "font4x4.h"
#include "weather_icon.h"

// Util macros
#define TOUCHED(x, touched) touched & 1 << TOUCH_PADS[x]
#define REVERSE_BITS(b) (((b)&1?128:0)|((b)&2?64:0)|((b)&4?32:0)|((b)&8?16:0)|((b)&16?8:0)|((b)&32?4:0)|((b)&64?2:0)|((b)&128?1:0))

int handle_touched(uint8_t touched) {
    if(TOUCHED(0, touched)) {
        display_mode = &display_time;
    } else if(TOUCHED(1, touched)) {
        display_mode = &display_weather;
    } else if(TOUCHED(2, touched)) {
        display_mode = &display_alarm;
    } else {
        return 0;
    }
    return 1;
}

display_mode_t display_mode = &display_time;

void display_time(uint8_t touched) {
    // Handle touched
    if(TOUCHED(0, touched)) {
        display_mode = &display_date;
    } else handle_touched(touched);

    time_t now = time(NULL);
    struct tm* local = localtime(&now);

    uint8_t mat[NUM_MATS][8] = {0};
    memcpy(mat[0], font8x8_basic[local->tm_hour / 10 + '0'], 8);
    memcpy(mat[1], font8x8_basic[local->tm_hour % 10 + '0'], 8);
    memcpy(mat[2], font8x8_basic[local->tm_min / 10 + '0'], 8);
    memcpy(mat[3], font8x8_basic[local->tm_min % 10 + '0'], 8);
    led_send_matrix(mat);
}

void display_date(uint8_t touched) {
    // Handle touched
    handle_touched(touched);

    time_t now = time(NULL);
    struct tm* local = localtime(&now);

    uint8_t mat[NUM_MATS][8] = {0};
    memcpy(mat[0], font8x8_basic[(local->tm_mon+1) / 10 + '0'], 8);
    memcpy(mat[1], font8x8_basic[(local->tm_mon+1) % 10 + '0'], 8);
    memcpy(mat[2], font8x8_basic[local->tm_mday / 10 + '0'], 8);
    memcpy(mat[3], font8x8_basic[local->tm_mday % 10 + '0'], 8);

    // Add a seperator between month and day
    mat[1][3] |= 0b00000001; mat[1][4] |= 0b00000001;
    mat[2][3] |= 0b10000000; mat[2][4] |= 0b10000000;

    led_send_matrix(mat);
}

void display_weather(uint8_t touched) {
    // Handle touched
    if(TOUCHED(1, touched)) {
        display_mode = &display_weather_gadget;
    } else handle_touched(touched);

    weather_t *weather = weather_get();
    uint8_t mat[NUM_MATS][8] = {0};

    // Weather icon
    memcpy(mat[0], weather_icon(atoi(weather->icon)), 8);

    int temp = (int) roundf(weather->temp) % 100;
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

void display_weather_gadget(uint8_t touched) {
    // Handle touched
    handle_touched(touched);

    weather_t *weather = weather_get();
    uint8_t mat[NUM_MATS][8] = {0};

    // Icon
    memcpy(mat[0], weather_icon(atoi(weather->icon)), 8);

    // Humidity
    {
        int humidity = weather->humidity;
        if(humidity > 99) humidity = 99;
        font4x4_merge((int[4]) {'H', 'M', humidity / 10 + '0', humidity % 10 + '0'}, mat[1]);
    }

    // Clouds
    {
        int clouds = weather->clouds;
        if(clouds > 99) clouds = 99;
        font4x4_merge((int[4]) {'C', 'L', clouds / 10 + '0', clouds % 10 + '0'}, mat[2]);
    }

    // Wind
    {
        int wind = (int) roundf(weather->wind * 1.943844f);
        font4x4_merge((int[4]) {'K', 'T', wind / 10 + '0', wind % 10 + '0'}, mat[3]);
    }

    led_send_matrix(mat);
}

void display_alarm(uint8_t touched) {
    // Struct to help setting alarm
    static struct {
        alarm_t alarm;
        int index;
        int editing;
        enum {
            HOUR, MIN, SUN, MON, TUE, WED, THU, FRI, SAT, ENA, DEL, MAX
        } selection;
    } ctl;

    // Get alarms
    size_t len = 0;
    const alarm_t *alarms = alarm_get(&len);
    uint8_t mat[NUM_MATS][8] = {0};

    // Handle touched
    if(ctl.editing) { // Edit-mode
        // Change value
        if(TOUCHED(0, touched) || TOUCHED(1, touched)) {
            int inc = TOUCHED(0, touched);
            int time = ctl.alarm.time; // signed copy of alarm time

            switch(ctl.selection) {
                case HOUR:
                    time += inc ? 60 : -60;
                    break;
                case MIN:
                    time += inc ? 1 : -1;
                    break;
                case SUN: case MON: case TUE: case WED: case THU: case FRI: case SAT:
                    ctl.alarm.days ^= 1 << (ctl.selection - SUN);
                    break;
                case ENA:
                    ctl.alarm.enabled = !ctl.alarm.enabled;
                    break;
                case DEL:
                    alarm_delete(ctl.index);
                    ctl.editing = 0;
                    break;
                default: break;
            }

            // Constrain time and assign to alarm
            if(time < 0) time += 24*60;
            else if(time >= 24*60) time -= 24*60;
            ctl.alarm.time = time;

        // Switch selection
        } else if(TOUCHED(2, touched)) {
            ctl.selection++;
            if(ctl.selection >= MAX) ctl.selection = 0;

        // Trigger edit-mode
        } else if(TOUCHED(3, touched)) {
            ctl.editing = !ctl.editing;
            // Save alarm
            if(ctl.index < len)
                alarm_set(ctl.index, ctl.alarm);
            else
                alarm_add(ctl.alarm);
        }
    } else { // Non-edit mode
        // Switch mode
        if(TOUCHED(0, touched)) {
            ctl.index = 0;
            display_mode = &display_time;
            return;
        } else if(TOUCHED(1, touched)) {
            ctl.index = 0;
            display_mode = &display_weather;
            return;

        // Switch index
        } else if(TOUCHED(2, touched)) {
            ctl.index++;
            if(ctl.index > len) ctl.index = 0;

        // Trigger edit-mode
        } else if(TOUCHED(3, touched)) {
            ctl.alarm = ctl.index < len ? alarms[ctl.index] : (alarm_t) {0};
            ctl.selection = HOUR;
            ctl.editing = !ctl.editing;
        }
    }

    // Display
    if(!ctl.editing && ctl.index == len) {
        for(int i = 0; i < NUM_MATS; i++) {
            memcpy(mat[i], font8x8_basic['-'], 8);
        }
    } else {
        // Get alarm values
        const alarm_t *alarm = ctl.editing ? &ctl.alarm : &alarms[ctl.index];
        int hour = alarm->time / 60;
        int min = alarm->time % 60;

        // Use first 2 matrix for hours
        memcpy(mat[0], font8x8_basic[hour / 10 + '0'], 8);
        memcpy(mat[1], font8x8_basic[hour % 10 + '0'], 8);

        // Squeeze the minutes in the third
        font4x4_merge((int[4]) {min / 10 + '0', min % 10 + '0'}, mat[2]);

        // Use the bottom of third matrix for overview
        mat[2][6] = mat[2][7] = REVERSE_BITS(alarm->days) | alarm->enabled;

        if(ctl.editing) {
            // Show selection
            if(ctl.selection >= SUN && ctl.selection <= ENA) {
                mat[2][5] = 0x80 >> (ctl.selection - SUN);
            } else switch(ctl.selection) {
                case HOUR: mat[2][4] |= 0b10000000; break;
                case MIN: mat[2][3] |= 0b00001000; break;
                default: break;
            }

            // Last matrix for displaying current selection
            int *letters;
            switch(ctl.selection) {
                case HOUR: letters = (int[4]) {'H','R'}; break;
                case MIN: letters = (int[4]) {'M','I'}; break;
                case SUN: letters = (int[4]) {'S','U'}; break;
                case MON: letters = (int[4]) {'M','O'}; break;
                case TUE: letters = (int[4]) {'T','U'}; break;
                case WED: letters = (int[4]) {'W','E'}; break;
                case THU: letters = (int[4]) {'T','H'}; break;
                case FRI: letters = (int[4]) {'F','R'}; break;
                case SAT: letters = (int[4]) {'S','A'}; break;
                case ENA: letters = (int[4]) {'E','N'}; break;
                case DEL: letters = (int[4]) {'D','L'}; break;
                default: letters = (int[4]) {0}; break;
            }
            font4x4_merge(letters, mat[3]);

            // Fill the bottom so it doesn't look empty
            mat[3][5] = mat[3][6] = 0x7E;
        } else {
            time_t now = time(NULL);
            struct tm* local = localtime(&now);

            // Big alarm clock icon
            if(alarm->enabled && alarm->days & 1 << local->tm_wday) {
                mat[3][0] = 0b00001000;
                mat[3][1] = 0b00011100;
                mat[3][2] = 0b00111110;
                mat[3][3] = 0b00111110;
                mat[3][4] = 0b00111110;
                mat[3][5] = 0b01111111;
                mat[3][6] = 0b00001000;
            } else {
                mat[3][0] = 0b00001000;
                mat[3][1] = 0b00011100;
                mat[3][2] = 0b00111010;
                mat[3][3] = 0b00110110;
                mat[3][4] = 0b00101110;
                mat[3][5] = 0b01011111;
                mat[3][6] = 0b00001000;
            }
        }
    }

    led_send_matrix(mat);
}
