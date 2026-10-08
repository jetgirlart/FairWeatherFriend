#pragma once
using gpio_num_t=int;
using esp_err_t=int;
using esp_sleep_wakeup_cause_t=int;
constexpr int ESP_OK=0,ESP_SLEEP_WAKEUP_EXT0=2;
inline esp_err_t esp_sleep_enable_ext0_wakeup(int,int) {return ESP_OK;}
inline void esp_deep_sleep_start() {}
inline int esp_sleep_get_wakeup_cause() {return 0;}
