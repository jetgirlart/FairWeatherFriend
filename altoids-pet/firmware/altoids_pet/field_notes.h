#pragma once
#include "save.h"

// Age zero is newest. Returned pointers remain owned by the fixed buddy save.
inline const FieldNote *fieldNoteAt(const BuddySaveData &s, uint8_t age) {
  if (age >= s.fieldNoteCount || s.fieldNoteNext >= FIELD_NOTE_COUNT) return nullptr;
  return &s.fieldNotes[(s.fieldNoteNext + FIELD_NOTE_COUNT - 1 - age) % FIELD_NOTE_COUNT];
}
inline void appendFieldNote(BuddySaveData &s, const FieldNote &note) {
  s.fieldNotes[s.fieldNoteNext] = note;
  s.fieldNoteNext = (s.fieldNoteNext + 1) % FIELD_NOTE_COUNT;
  if (s.fieldNoteCount < FIELD_NOTE_COUNT) ++s.fieldNoteCount;
}
inline bool addedGearVariant(const BuddySaveData &before, const BuddySaveData &after) {
  for (uint8_t i = 0; i < 7; ++i)
    if ((after.unlockedVariants[i] & ~before.unlockedVariants[i]) & ~1UL) return true;
  return false; // Original ownership is reported separately as NEW_GEAR_UNLOCK.
}
inline uint32_t observationOutcomes(const BuddySaveData &before, const BuddySaveData &after) {
  uint32_t flags = 0;
  if (after.discoveredWeather & ~before.discoveredWeather) flags |= NEW_WEATHER_DISCOVERY;
  if (after.highestTemperatureAt != before.highestTemperatureAt) flags |= NEW_HIGH_TEMP;
  if (after.lowestTemperatureAt != before.lowestTemperatureAt) flags |= NEW_LOW_TEMP;
  const uint32_t recordFlags[RECORD_COUNT] = {NEW_WIND_RECORD, NEW_GUST_RECORD,
    NEW_HUMIDITY_RECORD, NEW_PRESSURE_RECORD, NEW_PRESSURE_RECORD, NEW_PRECIP_RECORD};
  for (uint8_t i = 0; i < RECORD_COUNT; ++i)
    if (after.records[i].timestamp != before.records[i].timestamp) flags |= recordFlags[i];
  if (after.unlockedGear & ~before.unlockedGear) flags |= NEW_GEAR_UNLOCK;
  if (addedGearVariant(before, after)) flags |= NEW_GEAR_VARIANT;
  return flags;
}
