#include <stdio.h>

#include <esp_log.h>
#include <esp_http_client.h>
#include <frozen.h>

#include "weather.h"
#include "secret.h"

#define WEATHER_URL "http://api.openweathermap.org/data/2.5/weather"

static const char *TAG = "WEATHER";
esp_http_client_handle_t client = NULL;

void init_weather() {
    char url[110];
    sprintf(url, "%s?units=metric&appid=%s&id=%s", WEATHER_URL, WEATHER_API_KEY, WEATHER_CITY_ID);
    esp_http_client_config_t http_config = {
        .url = url,
        .method = HTTP_METHOD_GET
    };
    client = esp_http_client_init(&http_config);
}

void get_weather(weather_t *data) {
    esp_http_client_open(client, 0);

    if(esp_http_client_fetch_headers(client) <= 0 ||
       esp_http_client_get_status_code(client) != 200) {

        ESP_LOGW(TAG, "Cannot retrieve weather data");
        return;
    }

    int len = esp_http_client_get_content_length(client);
    char *buff = alloca(len);
    len = esp_http_client_read(client, buff, len);

    if(len <= 0) {
        ESP_LOGW(TAG, "Cannot retrieve weather data");
        return;
    }

    int parsed = 0;

    struct json_token token;
    if(json_scanf_array_elem(buff, len, ".weather", 0, &token)) {
        parsed = json_scanf(token.ptr, token.len,
                "{main: %s, icon: %s}",
                data->weather, data->icon);
    }

    if(parsed > 0) {
        parsed = json_scanf(buff, len,
                "{"
                    "main: {"
                        "temp: %f,"
                        "humidity: %d"
                    "},"
                    "wind: {speed: %f},"
                    "clouds: {all: %d},"
                    "sys: {"
                        "sunrise: %ld,"
                        " sunset: %ld"
                    "}"
                "}",
                &data->temp, &data->humidity, &data->wind, &data->clouds, &data->sunrise, &data->sunset);
        printf("parsed %d", parsed);
    }

    if(parsed <= 0) {
        ESP_LOGW(TAG, "Cannot parse weather data");
    }

    esp_http_client_close(client);
}
