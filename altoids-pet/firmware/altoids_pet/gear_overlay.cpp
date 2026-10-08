#include "gear_overlay.h"
#include "gear_sprites.h"
#include "journal.h"
#include "display.h"

void drawEquippedGear(int x, int y, bool crouching, bool liftedFeet,
                      GearId foregroundAccessory) {
  // Never display an unread/unsupported save's equipment.
  if (!journalAvailable()) return;
  // The existing weather animation temporarily supplies the same accessory.
  // Keep its animation visible without drawing two umbrellas/scarves.
  GearId equipped = getBuddySave().equippedGear;
  if (equipped == foregroundAccessory) return;
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
