#include "gear_overlay.h"
#include "gear_sprites.h"
#include "gear.h"
#include "journal.h"
#include "display.h"

namespace {
void drawGearItem(GearId equipped, int x, int y, bool crouching, bool liftedFeet) {
  const uint8_t *art = nullptr;
  const uint8_t *mask = nullptr;
  switch (equipped) {
    case GearId::FIELD_CAP: art = GEAR_FIELD_CAP; mask = GEAR_FIELD_CAP_MASK; break;
    // Outline lenses leave the existing blink/look eyes visible through them.
    case GearId::SUNGLASSES: art = GEAR_SUNGLASSES; break;
    case GearId::UMBRELLA:
      art = GEAR_UMBRELLA; mask = GEAR_UMBRELLA_MASK;
      x += 40; y -= 10;
      crouching = false; // Rigid accessory; only torso clothing compresses.
      break;
    case GearId::RAINCOAT: art = GEAR_RAINCOAT; mask = GEAR_RAINCOAT_MASK; break;
    case GearId::WINTER_SCARF: art = GEAR_WINTER_SCARF; mask = GEAR_WINTER_SCARF_MASK; break;
    case GearId::WINTER_COAT: art = GEAR_WINTER_COAT; mask = GEAR_WINTER_COAT_MASK; break;
    case GearId::BOOTS:
      art = GEAR_BOOTS; mask = GEAR_BOOTS_MASK;
      // BOUNCE artwork lifts the paws one source pixel, in addition to the hop.
      if (liftedFeet) y -= 2;
      break;
    case GearId::NONE:
    default: return;
  }
  // Hat/glasses stay rigid while the ear tip twitches independently. All layers
  // use the base's exact 2x bitmap expansion and body-crouch row mapping.
  if (mask) drawKitsuneSprite(mask, x, y, 0, crouching, SH110X_BLACK);
  drawKitsuneSprite(art, x, y, 0, crouching);
}

} // namespace
void drawEquippedGear(int x, int y, bool crouching, bool liftedFeet, GearId foregroundAccessory) {
  if (!journalAvailable()) return;
  const GearSlot order[] = {GearSlot::BODY, GearSlot::FEET, GearSlot::NECK,
                            GearSlot::HEAD, GearSlot::FACE, GearSlot::PROP};
  for (GearSlot slot : order) {
    GearId gear = getBuddySave().equippedSlots[static_cast<uint8_t>(slot)];
    // Weather supplies its animated accessory above clothing, without duplicates.
    if (gear != foregroundAccessory && gearFitsSlot(gear, slot))
      drawGearItem(gear, x, y, crouching, liftedFeet);
  }
}
