#include <stdio.h>

#include <esp_log.h>
#include <esp_err.h>
#include <esp_http_client.h>
#include <frozen.h>

#include "weather.h"
#include "secret.h"
#include "error.h"

#define WEATHER_URL "http://api.openweathermap.org/data/2.5/weather"

static const char *TAG = "WEATHER";
esp_http_client_handle_t client = NULL;

static weather_t weather = {0};

esp_err_t weather_init() {
    // Generate query URL
    char url[110];
    sprintf(url, "%s?units=metric&appid=%s&id=%s", WEATHER_URL, WEATHER_API_KEY, WEATHER_CITY_ID);

    // Init HTTP client
    esp_http_client_config_t http_config = {
        .url = url,
        .method = HTTP_METHOD_GET
    };
    client = esp_http_client_init(&http_config);

    return client ? ESP_OK : ESP_FAIL;
}

esp_err_t weather_update() {
    // Open connection
    ERROR_CHECK_RETURN(esp_http_client_open(client, 0));

    // Check response status
    if(esp_http_client_fetch_headers(client) <= 0 ||
       esp_http_client_get_status_code(client) != 200) {
        ESP_LOGW(TAG, "Cannot retrieve weather data");
        esp_http_client_close(client);
        return ESP_FAIL;
    }

    // Get response data
    int len = esp_http_client_get_content_length(client);
    char *buff = alloca(len);
    len = esp_http_client_read(client, buff, len);
    esp_http_client_close(client);

    if(len <= 0) {
        ESP_LOGW(TAG, "Cannot retrieve weather data");
        return ESP_FAIL;
    }

    // Start parsing returned data
    int parsed = 0;

    struct json_token token;
    if(json_scanf_array_elem(buff, len, ".weather", 0, &token)) {
        parsed = json_scanf(token.ptr, token.len,
                "{main: %s, icon: %s}",
                weather.weather, weather.icon);
    }

    // For some reason doing them all at once doesn't work
    parsed += json_scanf(buff, len, "{main: {temp: %f, humidity: %d}}", &weather.temp, &weather.humidity);
    parsed += json_scanf(buff, len, "{wind: {speed: %f}}", &weather.wind);
    parsed += json_scanf(buff, len, "{clouds: {all: %d}}", &weather.clouds);
    parsed += json_scanf(buff, len, "{sys: {sunrise: %ld, sunset: %ld}}", &weather.sunrise, &weather.sunset);

    if(parsed < 8) { // 8 is number of member in weather_t
        ESP_LOGW(TAG, "Cannot parse weather data");
        return ESP_FAIL;
    }

    return ESP_OK;
}

weather_t *weather_get() {
    return &weather;
}
