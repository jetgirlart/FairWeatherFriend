#include "gear_overlay.h"
#include "gear_sprites.h"
#include "generated/gear_assets.h"
#include "gear.h"
#include "gear_variants.h"
#include "journal.h"
#include "display.h"

namespace {
void drawGearItem(GearId equipped, int x, int y, bool crouching, bool liftedFeet, uint8_t variant = 0) {
  const uint8_t *packed = nullptr;
  const uint8_t *art = nullptr;
  const uint8_t *mask = nullptr;
  int pixelOffsetY = 0;
  switch (equipped) {
    case GearId::FIELD_CAP:
#ifdef FWF_PNG_HAS_GEAR_FIELD_CAP
      packed = PNG_GEAR_FIELD_CAP_ROLES;
#else
      art = GEAR_FIELD_CAP; mask = GEAR_FIELD_CAP_MASK;
#endif
      break;
    case GearId::SUNGLASSES:
#ifdef FWF_PNG_HAS_GEAR_SUNGLASSES
      packed = PNG_GEAR_SUNGLASSES_ROLES;
#else
      art = GEAR_SUNGLASSES;
#endif
      break;
    case GearId::UMBRELLA:
      art = GEAR_UMBRELLA; mask = GEAR_UMBRELLA_MASK;
      x += 48; y -= 12;
      crouching = false; // Rigid accessory; only torso clothing compresses.
      break;
    case GearId::RAINCOAT: art = GEAR_RAINCOAT; mask = GEAR_RAINCOAT_MASK; break;
    case GearId::WINTER_SCARF: art = GEAR_WINTER_SCARF; mask = GEAR_WINTER_SCARF_MASK; break;
    case GearId::WINTER_COAT: art = GEAR_WINTER_COAT; mask = GEAR_WINTER_COAT_MASK; break;
    case GearId::BOOTS:
      art = GEAR_BOOTS; mask = GEAR_BOOTS_MASK;
      // BOUNCE artwork lifts the paws one source pixel, in addition to the hop.
      if (liftedFeet) pixelOffsetY = -2;
      break;
    case GearId::NONE:
    default: return;
  }
  // Hat/glasses stay rigid while the ear tip twitches independently. All layers
  // use the base's native 2x pixels and the shared body-crouch row mapping.
  if (packed) {
    // PNG alpha is authoritative: empty pixels leave the underlying pet intact.
    drawPackedPaletteSprite(packed, x, y, gearVariantPalette(equipped, variant),
                            0, crouching, pixelOffsetY);
    return;
  }
  if (mask) drawKitsuneSprite(mask, x, y, 0, crouching, COLOR_BACKGROUND, pixelOffsetY);
  drawPaletteGearSprite(art, x, y, gearVariantPalette(equipped, variant), patternedGearVariant(equipped, variant), crouching, pixelOffsetY);
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
      drawGearItem(gear, x, y, crouching, liftedFeet, getBuddySave().equippedVariants[uint8_t(slot)]);
  }
}

void drawWeatherGear(GearId gear, int x, int y, bool crouching) {
  uint8_t variant = 0; GearSlot slot = gearSlot(gear);
  if (journalAvailable() && uint8_t(slot) < GEAR_SLOT_COUNT && getBuddySave().equippedSlots[uint8_t(slot)] == gear)
    variant = getBuddySave().equippedVariants[uint8_t(slot)];
  drawGearItem(gear, x, y, crouching, false, variant);
}
