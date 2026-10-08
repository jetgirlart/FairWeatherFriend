#pragma once
using gpio_num_t=int;
void gpio_hold_dis(gpio_num_t);
void gpio_hold_en(gpio_num_t);
void gpio_deep_sleep_hold_en();

void gpio_deep_sleep_hold_dis();
