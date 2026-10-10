#include <assert.h>
#include "Arduino.h"
#include "display.h"
#include "journal.h"
#include "pet.h"
#include "weather.h"
#include "gear_sprites.h"
#include "generated/gear_assets.h"
SerialType Serial;
PetPalette petPalette;
unsigned long fakeMillis=0;
uint16_t pixels[240*240]={};
std::vector<DrawCall> bitmapCalls;
unsigned pushes=0,clears=0,umbrellas=0,scarves=0;
DisplaySurface fakeDisplay;
DisplaySurface &display=fakeDisplay;
ScreenMode currentScreen=HOME;
BuddySaveData data;
bool available=true;
const BuddySaveData &getBuddySave(){return data;}
bool journalAvailable(){return available;}
bool timeValid=true,weatherValid=true;
int currentHour=12;
WeatherState weatherState=WEATHER_CLEAR;
bool isDaylight(){return currentHour>=7 && currentHour<19;}
void soundPetInteraction(){}
void drawHome(){display.clearDisplay();setSpriteOrigin(72,56);drawPet(0,petReacting?-6:0,isPetSleeping(),blinking);display.display();}
#include "../../firmware/altoids_pet/palette.cpp"
#include "../../firmware/altoids_pet/sprites.cpp"
#include "../../firmware/altoids_pet/kitsune_assets.cpp"
#include "../../firmware/altoids_pet/gear_sprites.cpp"
#include "../../firmware/altoids_pet/gear.cpp"
#include "../../firmware/altoids_pet/gear_variants.cpp"
bool equipJournalGear(GearSlot,GearId){return false;}
bool equipJournalGearVariant(GearSlot,GearId,uint8_t){return false;}
#include "../../firmware/altoids_pet/gear_overlay.cpp"
#include "../../firmware/altoids_pet/pet.cpp"
void rest(){
 currentScreen=HOME;currentHour=12;available=true;weatherValid=true;petReacting=false;
 idlePaused=false;idleMood=PetMood::CALM;petState.mood=PetMood::CALM;
 idleAction=IdleAction::REST;idleStep=0;blinking=false;
 nextBlinkTime=fakeMillis+100000;idleBlinkDeadline=nextBlinkTime;
 reactionWeather=weatherState;reactionDaylight=isDaylight();setSpriteOrigin(72,56);
}
struct Item{GearId id;const uint8_t *art,*mask;};
const Item items[]={
 {GearId::FIELD_CAP,GEAR_FIELD_CAP,GEAR_FIELD_CAP_MASK},
 {GearId::SUNGLASSES,GEAR_SUNGLASSES,nullptr},
 {GearId::UMBRELLA,GEAR_UMBRELLA,GEAR_UMBRELLA_MASK},
 {GearId::RAINCOAT,GEAR_RAINCOAT,GEAR_RAINCOAT_MASK},
 {GearId::WINTER_SCARF,GEAR_WINTER_SCARF,GEAR_WINTER_SCARF_MASK},
 {GearId::WINTER_COAT,GEAR_WINTER_COAT,GEAR_WINTER_COAT_MASK},
 {GearId::BOOTS,GEAR_BOOTS,GEAR_BOOTS_MASK}};
void expectedMask(std::vector<uint16_t>&out,const uint8_t *bits,int x,int y,uint16_t color){
 if(!bits)return;
 for(int sy=0;sy<48;++sy)for(int sx=0;sx<48;++sx)if(bits[sy*6+sx/8]&(0x80>>(sx%8)))
  for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){
   int X=x+2*sx+dx,Y=y+2*sy+dy;assert(X>=0 && X<240 && Y>=0 && Y<240);out[Y*240+X]=color;
  }
}
void expectedItem(std::vector<uint16_t>&out,GearId gear,int x,int y,bool lifted=false){
 for(auto item:items)if(item.id==gear){
  if(gear==GearId::UMBRELLA){x+=72;y-=18;}
  if(gear==GearId::BOOTS && lifted)y-=2;
  const uint8_t *packed=gear==GearId::FIELD_CAP?PNG_GEAR_FIELD_CAP_ROLES:
                         gear==GearId::SUNGLASSES?PNG_GEAR_SUNGLASSES_ROLES:nullptr;
  if(packed){
   auto palette=gearVariantPalette(gear,0);
   const uint16_t colors[]={0,palette.outline,palette.primary,palette.accent,palette.detail};
   for(int sy=0;sy<48;sy++)for(int sx=0;sx<48;sx++){
    int i=sy*48+sx;uint8_t role=i%2?packed[i/2]&15:packed[i/2]>>4;
    if(role)for(int dy=0;dy<2;dy++)for(int dx=0;dx<2;dx++)out[(y+2*sy+dy)*240+x+2*sx+dx]=colors[role];
   }
   return;
  }
  expectedMask(out,item.mask,x,y,COLOR_BACKGROUND);
  expectedMask(out,item.art,x,y,gearVariantPalette(gear,0).primary);
 }
}
void checkPixels(const std::vector<uint16_t>&expected){assert(memcmp(expected.data(),pixels,sizeof(pixels))==0);}
unsigned brightness(uint16_t color){
 unsigned r=((color>>11)&31)*255/31,g=((color>>5)&63)*255/63,b=(color&31)*255/31;
 return 2126*r+7152*g+722*b;
}
void checkRoleOrder(const SpritePalette &palette){
 assert(palette.outline==0x0000 && palette.detail==0xFFFF);
 assert(brightness(palette.outline)<brightness(palette.primary));
 assert(brightness(palette.primary)<brightness(palette.accent));
 assert(brightness(palette.accent)<brightness(palette.detail));
}
int main(){
 for(uint8_t id=0;id<FUR_PALETTE_COUNT;id++)checkRoleOrder(furPalette(static_cast<FurPaletteId>(id)));
 for(auto item:items)for(uint8_t variant=0;variant<gearVariantCount(item.id);variant++)checkRoleOrder(gearVariantPalette(item.id,variant));
 checkRoleOrder(gearVariantPalette(GearId::NONE,255));
 // Active PNG gear layers use the same semantic colors and pixel transform.
 const GearId pngItems[]={GearId::FIELD_CAP,GearId::SUNGLASSES};
 const uint8_t *pngFrames[]={PNG_GEAR_FIELD_CAP_ROLES,PNG_GEAR_SUNGLASSES_ROLES};
 for(unsigned asset=0;asset<2;asset++)for(uint8_t variant=0;variant<gearVariantCount(pngItems[asset]);variant++){
  for(auto &value:pixels)value=COLOR_WARM;
  setSpriteOrigin(72,56);const auto &palette=gearVariantPalette(pngItems[asset],variant);
  assert(palette.detail==0xFFFF);
  drawPackedPaletteSprite(pngFrames[asset],0,16,palette,0,false,-2);
  const uint16_t expected[]={COLOR_WARM,palette.outline,palette.primary,palette.accent,palette.detail};
  for(int y=0;y<96;y++)for(int x=0;x<96;x++){
   int index=(y/2)*48+x/2;uint8_t packed=pngFrames[asset][index/2];
   uint8_t role=index%2?packed&15:packed>>4;
   assert(pixels[(78+y)*240+72+x]==expected[role]);
  }
  assert(pixels[77*240+72]==COLOR_WARM); // Transparent/offset pixels do not erase the base.
 }
 initializePetState();initializeAnimations();
 for(auto item:items){
  rest();for(auto &slot:data.equippedSlots)slot=GearId::NONE;
  data.equippedSlots[static_cast<uint8_t>(gearSlot(item.id))]=item.id;
  for(int i=0;i<240*240;++i)pixels[i]=i%5==0?COLOR_WARM:COLOR_BACKGROUND;
  std::vector<uint16_t> expected(pixels,pixels+240*240);
  expectedItem(expected,item.id,72,80);unsigned transfers=pushes;
  drawEquippedGear(0,16,false,false);checkPixels(expected);assert(pushes==transfers);
 }
 const GearId order[]={GearId::RAINCOAT,GearId::BOOTS,GearId::WINTER_SCARF,
                       GearId::FIELD_CAP,GearId::SUNGLASSES,GearId::UMBRELLA};
 for(auto &slot:data.equippedSlots)slot=GearId::NONE;
 for(auto gear:order)data.equippedSlots[static_cast<uint8_t>(gearSlot(gear))]=gear;
 for(auto screen:{HOME,FOCUS_SCREEN,TIMER_DONE})for(bool sleeping:{false,true}){
  rest();currentScreen=screen;int y=screen==HOME?56:screen==FOCUS_SCREEN?88:62;
  setSpriteOrigin(72,y);display.clearDisplay();drawPet(0,0,sleeping,false);
  std::vector<uint16_t> actual(pixels,pixels+240*240);
  display.clearDisplay();drawColoredKitsune(sleeping?KITSUNE_SLEEP:screen==TIMER_DONE?KITSUNE_HAPPY:screen==FOCUS_SCREEN?KITSUNE_FOCUS:KITSUNE_IDLE,0,16,data.furPalette);
  std::vector<uint16_t> expected(pixels,pixels+240*240);
  for(auto gear:order)expectedItem(expected,gear,72,y+24);
  assert(expected==actual);
 }
 // Every pose and every palette with all slots: base transform and clothing agree.
 for(uint8_t id=0;id<5;++id)for(auto action:{IdleAction::LOOK_LEFT,IdleAction::LOOK_RIGHT,IdleAction::BOUNCE,IdleAction::EAR_TWITCH,IdleAction::STORM_CROUCH,IdleAction::SNOW_SHIVER}){
  rest();data.furPalette=static_cast<FurPaletteId>(id);idleAction=action;idleStep=1;
  weatherState=reactionWeather=action==IdleAction::STORM_CROUCH?WEATHER_STORM:action==IdleAction::SNOW_SHIVER?WEATHER_SNOW:WEATHER_CLEAR;
  display.clearDisplay();drawPet(0,0,false,false);
  std::vector<uint16_t> actual(pixels,pixels+240*240);
  int x=action==IdleAction::SNOW_SHIVER?1:0,y=action==IdleAction::BOUNCE?-2:action==IdleAction::STORM_CROUCH?3:0;
  const uint8_t *frame=action==IdleAction::LOOK_LEFT?KITSUNE_LOOK_LEFT:action==IdleAction::LOOK_RIGHT?KITSUNE_LOOK_RIGHT:action==IdleAction::BOUNCE?KITSUNE_BOUNCE:action==IdleAction::STORM_CROUCH?KITSUNE_BLINK:KITSUNE_IDLE;
  display.clearDisplay();drawColoredKitsune(frame,x,y+16,data.furPalette,action==IdleAction::EAR_TWITCH?2:0,action==IdleAction::STORM_CROUCH);
  drawEquippedGear(x,y+16,action==IdleAction::STORM_CROUCH,frame==KITSUNE_BOUNCE,action==IdleAction::SNOW_SHIVER?GearId::WINTER_SCARF:GearId::NONE);
  if(action==IdleAction::SNOW_SHIVER)drawWeatherGear(GearId::WINTER_SCARF,x,y+16);
  assert(actual==std::vector<uint16_t>(pixels,pixels+240*240));
 }
 // PNG transparency preserves each expression; opaque pixels use exact palette roles.
 for(auto frame:{KITSUNE_IDLE,KITSUNE_BLINK,KITSUNE_LOOK_LEFT,KITSUNE_LOOK_RIGHT,KITSUNE_SLEEP,KITSUNE_FOCUS}){
  rest();display.clearDisplay();drawColoredKitsune(frame,0,16,data.furPalette);
  std::vector<uint16_t> expected(pixels,pixels+240*240);
  expectedItem(expected,GearId::SUNGLASSES,72,80);
  drawGearItem(GearId::SUNGLASSES,0,16,false,false);checkPixels(expected);
 }
 for(uint8_t id=0;id<FUR_PALETTE_COUNT;id++)assert(furPalette(static_cast<FurPaletteId>(id)).detail==0xFFFF);
 assert(gearVariantPalette(GearId::NONE,255).detail==0xFFFF);
 // Weather reaction draws one matching prop and restores equipped art afterward.
 rest();weatherState=reactionWeather=WEATHER_RAIN;idleAction=IdleAction::UMBRELLA;idleStep=1;
 display.clearDisplay();drawPet(0,0,false,false);std::vector<uint16_t> reaction(pixels,pixels+240*240);
 display.clearDisplay();drawColoredKitsune(KITSUNE_IDLE,0,16,data.furPalette);
 drawEquippedGear(0,16,false,false,GearId::UMBRELLA);drawWeatherGear(GearId::UMBRELLA,0,17);
 assert(reaction==std::vector<uint16_t>(pixels,pixels+240*240));
 weatherState=WEATHER_CLEAR;display.clearDisplay();drawPet(0,0,false,false);
 std::vector<uint16_t> restored(pixels,pixels+240*240);rest();display.clearDisplay();drawPet(0,0,false,false);
 assert(restored==std::vector<uint16_t>(pixels,pixels+240*240));
 available=false;std::vector<uint16_t> before(pixels,pixels+240*240);drawEquippedGear(0,16,false,false);checkPixels(before);
 rest();lastAnimationTime=fakeMillis;unsigned transfers=pushes;
 fakeMillis+=249;updateAnimations();assert(pushes==transfers);fakeMillis++;updateAnimations();assert(pushes==transfers+1);
 // Every variant reuses the identical bitmap and mask footprint; only RGB565 changes.
 for(auto item:items){
  rest();display.clearDisplay();drawGearItem(item.id,0,16,false,false,0);std::vector<uint16_t> original(pixels,pixels+240*240);
  for(uint8_t variant=1;variant<gearVariantCount(item.id);variant++){
   display.clearDisplay();drawGearItem(item.id,0,16,false,false,variant);
   bool changed=false;
   for(size_t i=0;i<original.size();i++){
    assert((original[i]==COLOR_BACKGROUND)==(pixels[i]==COLOR_BACKGROUND));
    if(original[i]!=pixels[i])changed=true;
   }
   assert(changed);
  }
 }
 puts("PASS: native gear masks/transparency/2x scaling, independent layer order, home/focus/DONE/sleep, all palettes and idle poses, authored PNG glasses and white DETAIL, shared weather accessories, protected saves and unchanged animation cadence.");
}
