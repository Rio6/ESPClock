#pragma once

typedef void (*display_mode_t)();
extern display_mode_t display_mode;

void display_time();
void display_date();
void display_weather();
void display_weather_gadget();
void display_alarm();
