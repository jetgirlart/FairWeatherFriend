#include <assert.h>
#include "Arduino.h"
#include "display.h"
#include "journal.h"
#include "pet.h"
#include "weather.h"
#include "gear_sprites.h"

SerialType Serial;
PetPalette petPalette;
unsigned long fakeMillis = 0;
uint16_t pixels[128*128] = {};
std::vector<DrawCall> bitmapCalls;
unsigned pushes=0,clears=0,umbrellas=0,scarves=0;
DisplaySurface fakeDisplay;
DisplaySurface &display=fakeDisplay;
ScreenMode currentScreen=HOME;
BuddySaveData data;
bool available=true;
const BuddySaveData &getBuddySave() {return data;}
bool journalAvailable() {return available;}
bool timeValid=true,weatherValid=true;
int currentHour=12;
WeatherState weatherState=WEATHER_CLEAR;
bool isDaylight() {return currentHour>=7 && currentHour<19;}
void soundPetInteraction() {}
void drawHome() {display.clearDisplay();drawPet(40,petReacting?23:29,isPetSleeping(),blinking);display.display();}

#include "../../firmware/altoids_pet/sprites.cpp"
#include "../../firmware/altoids_pet/gear_sprites.cpp"
#include "../../firmware/altoids_pet/gear.cpp"
bool equipJournalGear(GearSlot, GearId) { return false; }
#include "../../firmware/altoids_pet/gear_overlay.cpp"
#include "../../firmware/altoids_pet/pet.cpp"

void setOnlyGear(GearId gear) {
  for (auto &item : data.equippedSlots) item = GearId::NONE;
  auto slot = gearSlot(gear);
  if (slot != GearSlot::COUNT) data.equippedSlots[static_cast<uint8_t>(slot)] = gear;
}
void rest() {
  currentScreen=HOME;currentHour=12;available=true;weatherValid=true;petReacting=false;
  idlePaused=false;idleMood=PetMood::CALM;petState.mood=PetMood::CALM;
  idleAction=IdleAction::REST;idleStep=0;blinking=false;
  nextBlinkTime=fakeMillis+100000;idleBlinkDeadline=nextBlinkTime;
  reactionWeather=weatherState;reactionDaylight=isDaylight();
  bitmapCalls.clear();umbrellas=scarves=0;
}
void checkTransparentLayer(GearId gear,const uint8_t *art,const uint8_t *mask) {
  setOnlyGear(gear);
  // A background pattern makes unintended transparent-pixel erasure visible.
  for(unsigned i=0;i<128*128;i++)pixels[i]=(i%5)==0;
  uint16_t before[sizeof(pixels)];memcpy(before,pixels,sizeof(pixels));
  bitmapCalls.clear();unsigned transfers=pushes;
  drawEquippedGear(40,45,false,false);
  assert(pushes==transfers && bitmapCalls.size()==(mask?2:1));
  int x=gear==GearId::UMBRELLA?80:40,y=gear==GearId::UMBRELLA?35:45;
  for(int Y=0;Y<128;Y++)for(int X=0;X<128;X++) {
    uint16_t expected=before[Y*128+X];int sx=(X-x)/2,sy=(Y-y)/2;
    if(X>=x && X<x+48 && Y>=y && Y<y+48) {
      unsigned index=sy*3+sx/8,bit=0x80>>(sx%8);
      if(mask && (mask[index]&bit))expected=COLOR_BACKGROUND;
      if(art[index]&bit)expected=petPalette.gear[static_cast<uint8_t>(gear)];
    }
    assert(pixels[Y*128+X]==expected);
  }
}
int main() {
  initializePetState();initializeAnimations();
  struct Item {GearId gear;const uint8_t *art,*mask;};
  const Item items[]={
    {GearId::FIELD_CAP,GEAR_FIELD_CAP,GEAR_FIELD_CAP_MASK},
    {GearId::SUNGLASSES,GEAR_SUNGLASSES,nullptr},
    {GearId::UMBRELLA,GEAR_UMBRELLA,GEAR_UMBRELLA_MASK},
    {GearId::RAINCOAT,GEAR_RAINCOAT,GEAR_RAINCOAT_MASK},
    {GearId::WINTER_SCARF,GEAR_WINTER_SCARF,GEAR_WINTER_SCARF_MASK},
    {GearId::WINTER_COAT,GEAR_WINTER_COAT,GEAR_WINTER_COAT_MASK},
    {GearId::BOOTS,GEAR_BOOTS,GEAR_BOOTS_MASK}
  };
  for(auto &item:items)checkTransparentLayer(item.gear,item.art,item.mask);
  rest();setOnlyGear(GearId::NONE);drawEquippedGear(40,45,false,false);assert(bitmapCalls.empty());
  data.equippedSlots[0]=static_cast<GearId>(99);drawEquippedGear(40,45,false,false);assert(bitmapCalls.empty());
  available=false;setOnlyGear(GearId::FIELD_CAP);drawEquippedGear(40,45,false,false);assert(bitmapCalls.empty());
  // All gear is in bounds for home/B/focus/DONE, sleep and storm poses.
  for(auto &item:items)for(auto screen:{HOME,FOCUS_SCREEN,TIMER_DONE})for(bool sleeping:{false,true}) {
    rest();setOnlyGear(item.gear);currentScreen=screen;
    int y=screen==HOME?29:screen==FOCUS_SCREEN?55:40;
    drawPet(40,y,sleeping,false);
    assert(bitmapCalls.front().x==40 && bitmapCalls.front().y==y+16);
    assert(bitmapCalls.size()==(item.mask?3:2));
  }
  // Every slot simultaneously: exact compositing order and shared animated anchors.
  for(auto &item:data.equippedSlots)item=GearId::NONE;
  const GearId outfit[]={GearId::RAINCOAT,GearId::BOOTS,GearId::WINTER_SCARF,
                         GearId::FIELD_CAP,GearId::SUNGLASSES,GearId::UMBRELLA};
  for(auto gear:outfit)data.equippedSlots[static_cast<uint8_t>(gearSlot(gear))]=gear;
  for(auto screen:{HOME,FOCUS_SCREEN,TIMER_DONE})for(bool sleeping:{false,true}) {
    rest();currentScreen=screen;int y=screen==HOME?29:screen==FOCUS_SCREEN?55:40;
    display.clearDisplay();drawPet(40,y,sleeping,false);
    assert(bitmapCalls.size()==12); // Base + five mask/art pairs + glasses.
    uint16_t rendered[sizeof(pixels)];memcpy(rendered,pixels,sizeof(pixels));
    display.clearDisplay();drawKitsuneSprite(sleeping?KITSUNE_SLEEP:screen==TIMER_DONE?KITSUNE_HAPPY:KITSUNE_IDLE,40,y+16);
    for(auto gear:outfit)drawGearItem(gear,40,y+16,false,false);
    assert(memcmp(rendered,pixels,sizeof(pixels))==0);
  }
  rest();idleAction=IdleAction::UMBRELLA;weatherState=reactionWeather=WEATHER_RAIN;
  drawPet(40,29,false,false);assert(bitmapCalls.size()==10 && umbrellas==1);
  rest();idleAction=IdleAction::SNOW_SHIVER;weatherState=reactionWeather=WEATHER_SNOW;
  drawPet(40,29,false,false);assert(bitmapCalls.size()==10 && scarves==2);
  rest();idleAction=IdleAction::STORM_CROUCH;weatherState=reactionWeather=WEATHER_STORM;
  drawPet(40,29,false,false);
  for(auto call:bitmapCalls)assert(call.x==80 ? call.y==38 && call.h==48 : call.y==48 && call.h==45);
  // Ear tip and gaze/blink scheduler survive: glasses don't clear eye interiors.
  for(auto sprite:{KITSUNE_IDLE,KITSUNE_BLINK,KITSUNE_LOOK_LEFT,KITSUNE_LOOK_RIGHT,KITSUNE_LOOK_UP}) {
    display.clearDisplay();drawKitsuneSprite(sprite,40,45);
    uint16_t before[sizeof(pixels)];memcpy(before,pixels,sizeof(pixels));
    setOnlyGear(GearId::SUNGLASSES);drawEquippedGear(40,45,false,false);
    for(int y=9;y<=11;y++)for(int x:{4,5,6,12,13,14})for(int dy=0;dy<2;dy++)for(int dx=0;dx<2;dx++)
      assert(pixels[(45+y*2+dy)*128+40+x*2+dx]==before[(45+y*2+dy)*128+40+x*2+dx]);
  }
  rest();setOnlyGear(GearId::FIELD_CAP);idleAction=IdleAction::EAR_TWITCH;idleStep=1;
  drawPet(40,29,false,false);assert(bitmapCalls.back().y==45);
  // Boots track the lifted-paw frame as well as the existing 1/2-pixel hop.
  rest();setOnlyGear(GearId::BOOTS);idleAction=IdleAction::BOUNCE;
  for(int step=0;step<3;step++) {
    idleStep=step;bitmapCalls.clear();drawPet(40,29,false,false);
    assert(bitmapCalls.front().y==(step==1?43:44));assert(bitmapCalls.back().y==bitmapCalls.front().y-2);
  }
  rest();setOnlyGear(GearId::RAINCOAT);idleAction=IdleAction::STORM_CROUCH;
  weatherState=reactionWeather=WEATHER_STORM;drawPet(40,29,false,false);
  for(auto call:bitmapCalls)assert(call.y==48 && call.h==45);
  // Snow's shiver offset applies equally to coat layers and the pet.
  rest();setOnlyGear(GearId::WINTER_COAT);idleAction=IdleAction::SNOW_SHIVER;
  weatherState=reactionWeather=WEATHER_SNOW;
  for(int step=0;step<4;step++) {
    idleStep=step;bitmapCalls.clear();drawPet(40,29,false,false);
    for(auto call:bitmapCalls)assert(call.x==(step%2?41:39));
  }
  // Existing weather supplies an equipped umbrella/scarf temporarily, not two.
  rest();setOnlyGear(GearId::UMBRELLA);idleAction=IdleAction::UMBRELLA;
  weatherState=reactionWeather=WEATHER_RAIN;drawPet(40,29,false,false);
  assert(bitmapCalls.size()==1 && umbrellas==1);
  rest();setOnlyGear(GearId::WINTER_SCARF);idleAction=IdleAction::SNOW_SHIVER;
  weatherState=reactionWeather=WEATHER_SNOW;drawPet(40,29,false,false);
  assert(bitmapCalls.size()==1 && scarves==2);
  // A stale weather action must not suppress equipped gear after weather changes.
  weatherState=WEATHER_CLEAR;bitmapCalls.clear();drawPet(40,29,false,false);assert(bitmapCalls.size()==3);
  rest();setOnlyGear(GearId::UMBRELLA);assert(interactWithPet());drawPet(40,23,false,false);
  assert(bitmapCalls[0].y==39 && bitmapCalls.back().x==80 && bitmapCalls.back().y==29);
  // Gear adds zero display transfers; the existing animation gate pushes once.
  rest();setOnlyGear(GearId::FIELD_CAP);lastAnimationTime=fakeMillis;unsigned before=pushes;
  fakeMillis+=249;updateAnimations();assert(pushes==before);
  fakeMillis+=1;updateAnimations();assert(pushes==before+1);
  puts("PASS: all gear/masks/transparent pixels, no-op NONE/protected IDs, bounds, sleep/focus/DONE, eye visibility, ear/bounce/shiver/crouch alignment, weather replacement, stale-weather recovery and unchanged frame gating.");
}
