#pragma once
#include "save.h"
const char *achievementName(AchievementId id);
const char *achievementDescription(AchievementId id);
uint32_t eligibleAchievements(const BuddySaveData &s);
// Returns newly earned bits; never removes an unlock or rewrites its timestamp.
uint32_t evaluateAchievements(BuddySaveData &s, int64_t timestamp);
// No notifications/sound; original threshold dates are unknown in older saves.
void reconstructAchievements(BuddySaveData &s);
inline bool achievementUnlocked(const BuddySaveData &s, AchievementId id) {
  return uint8_t(id) < ACHIEVEMENT_COUNT && (s.unlockedAchievements & (1UL << uint8_t(id)));
}
