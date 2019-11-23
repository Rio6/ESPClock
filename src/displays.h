#pragma once

typedef void (*display_mode_t)(uint8_t);
extern display_mode_t display_mode;

void display_time(uint8_t);
void display_date(uint8_t);
void display_weather(uint8_t);
void display_weather_gadget(uint8_t);
void display_alarm(uint8_t);
