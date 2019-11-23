#pragma once

#include "weather.h"

// Touch pads used
#define NUM_TOUCH 4
static const touch_pad_t TOUCH_PADS[NUM_TOUCH] = {TOUCH_PAD_NUM2, TOUCH_PAD_NUM3, TOUCH_PAD_NUM4, TOUCH_PAD_NUM5};

extern weather_t app_weather;
