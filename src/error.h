#pragma once
#define ERROR_CHECK_RETURN(x) do { \
        esp_err_t __err_rc = (x); \
        if(__err_rc != ESP_OK) return __err_rc; \
    } while(0)
