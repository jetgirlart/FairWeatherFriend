#pragma once
using gpio_num_t=int;
inline bool gnssTxHeld=false;
inline unsigned gnssHoldCalls=0,gnssReleaseCalls=0,gnssDeepHolds=0;
inline void gpio_hold_en(gpio_num_t pin){if(pin!=6)__builtin_trap();gnssTxHeld=true;++gnssHoldCalls;}
inline void gpio_hold_dis(gpio_num_t pin){if(pin!=6)__builtin_trap();gnssTxHeld=false;++gnssReleaseCalls;}
inline void gpio_deep_sleep_hold_en(){++gnssDeepHolds;}
