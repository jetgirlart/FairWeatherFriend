#pragma once
#include "esp_sleep.h"
constexpr int RTC_GPIO_MODE_INPUT_ONLY=0;
inline int rtc_gpio_init(int) {return ESP_OK;}
inline int rtc_gpio_deinit(int) {return ESP_OK;}
inline int rtc_gpio_set_direction(int,int) {return ESP_OK;}
inline int rtc_gpio_pulldown_dis(int) {return ESP_OK;}
inline int rtc_gpio_pullup_en(int) {return ESP_OK;}
inline int rtc_gpio_get_level(int pin) {return digitalRead(pin);}
