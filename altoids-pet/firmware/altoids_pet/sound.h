#pragma once
#include <stdint.h>

// Named transient events; never persisted or replayed after a reboot.
enum class SoundEvent : uint8_t {
  STARTUP, PET_INTERACTION, TIMER_COMPLETE, ACHIEVEMENT, GEAR_ITEM,
  GEAR_VARIANT, WEATHER_DISCOVERY, LIFETIME_RECORD, SEVERE_DISCOVERY,
  AMBIENT_RAIN, AMBIENT_STORM, AMBIENT_SNOW, AMBIENT_WIND, AMBIENT_NIGHT, COUNT
};
void initializeSound();
void updateSound();
void stopSound();
// Fixed bit queue coalesces repeated pending events of the same type.
void queueSoundEvent(SoundEvent event);
void soundPetInteraction();
void soundTimerDone();
void soundStartup();
