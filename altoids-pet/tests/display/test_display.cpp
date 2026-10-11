#include <assert.h>
#include <vector>
#include "Arduino.h"
#include "display_surface.h"
#include "Adafruit_ST7789.h"
#include "sprites.h"
SerialType Serial;
SPIClass SPI;
unsigned long fakeMillis=0;
int backlight=LOW, windows=0,transferred=0,writeY=0,writeX=0,windowW=0,windowH=0;
bool held=false,panelSleeping=false,displayOn=true,clockPrimed=false,spiTransaction=false;
std::vector<uint16_t> panel(240*240,0xFFFF);
void digitalWrite(int pin,int level){assert(pin==D9);backlight=level;}
void pinMode(int pin,int mode){assert(pin==D9 && mode==OUTPUT);}
void gpio_hold_dis(int pin){assert(pin==D9);held=false;}
void gpio_hold_en(int pin){assert(pin==D9 && backlight==LOW);held=true;}
void gpio_deep_sleep_hold_en(){assert(held);}
void gpio_deep_sleep_hold_dis(){assert(backlight==LOW);}
void SPIClass::begin(int sck,int miso,int mosi,int cs){assert(sck==D8 && miso==-1 && mosi==D10 && cs==-1);}
void SPIClass::beginTransaction(SPISettings settings){
 assert(settings.hz==40000000 && settings.order==MSBFIRST && settings.mode==SPI_MODE3);
 assert(backlight==LOW && !spiTransaction);spiTransaction=true;clockPrimed=true;
}
void SPIClass::endTransaction(){assert(spiTransaction);spiTransaction=false;}
Adafruit_ST7789::Adafruit_ST7789(SPIClass *spi,int cs,int dc,int rst){assert(spi==&SPI && cs==-1 && dc==D6 && rst==D4);}
void Adafruit_ST7789::init(int w,int h,int mode){assert(w==240 && h==240 && mode==SPI_MODE3 && clockPrimed && !spiTransaction);panelSleeping=false;displayOn=true;}
void Adafruit_ST7789::setRotation(int r){assert(r==0);}
void Adafruit_ST7789::setSPISpeed(uint32_t hz){assert(hz==40000000);}
void Adafruit_ST7789::startWrite(){}
void Adafruit_ST7789::setAddrWindow(int x,int y,int w,int h){assert(x>=0 && y>=0 && x+w<=240 && y+h<=240);writeX=x;writeY=y;windowW=w;windowH=h;windows++;}
void Adafruit_ST7789::writePixels(uint16_t *p,int n){assert(n==windowW && windowH>0);for(int i=0;i<n;i++)panel[writeY*240+writeX+i]=p[i];writeY++;windowH--;transferred+=n;}
void Adafruit_ST7789::endWrite(){assert(windowH==0);}
void Adafruit_ST7789::enableDisplay(bool on){assert(backlight==LOW);displayOn=on;}
void Adafruit_ST7789::enableSleep(bool on){assert(!displayOn && backlight==LOW);panelSleeping=on;}
#include "../../firmware/altoids_pet/display_surface.cpp"
#include "../../firmware/altoids_pet/palette.cpp"
#include "../../firmware/altoids_pet/sprites.cpp"
#include "../../firmware/altoids_pet/kitsune_assets.cpp"
#ifndef FWF_LAYOUT_TEST
int main(){
 initializeDisplayBus();initializeDisplay();assert(backlight==LOW && windows==0);
 display.setTextColor(COLOR_TEXT);display.setCursor(12,12);display.print("12:34");
 display.display();assert(backlight==HIGH && windows==225 && transferred==57600);
 auto first=panel;windows=transferred=0;display.display();assert(windows==0);
 display.clearDisplay();display.setCursor(12,12);display.print("12:34");
 assert(panel==first);display.display();assert(windows==0); // Never transfer RAM clear.
 display.drawPixel(239,239,COLOR_COOL);display.display();assert(windows==1 && transferred==256);
 assert(panel.back()==COLOR_COOL);windows=transferred=0;
 // Native 48x48 role data produces exact 2x2 pixels for every expression/palette.
 assert(resolveKitsuneFrame(KITSUNE_IDLE)==KITSUNE_IDLE);
 assert(resolveKitsuneFrame(KITSUNE_BLINK)==KITSUNE_BLINK);
#ifndef FWF_PNG_HAS_KITSUNE_SLEEP
 assert(resolveKitsuneFrame(KITSUNE_SLEEP)==KITSUNE_BLINK);
#endif
#ifndef FWF_PNG_HAS_KITSUNE_SLEEPY
 assert(resolveKitsuneFrame(KITSUNE_SLEEPY)==KITSUNE_BLINK);
#endif
#ifndef FWF_PNG_HAS_KITSUNE_LOOK_UP
 assert(resolveKitsuneFrame(KITSUNE_LOOK_UP)==KITSUNE_IDLE);
#endif
 const uint8_t *expressions[]={KITSUNE_IDLE,KITSUNE_BLINK,KITSUNE_LOOK_LEFT,KITSUNE_LOOK_RIGHT,
 KITSUNE_HAPPY,KITSUNE_EXCITED,KITSUNE_SLEEPY,KITSUNE_SLEEP,KITSUNE_BOUNCE,KITSUNE_LOOK_UP,KITSUNE_FOCUS};
 uint8_t roles[KITSUNE_PIXEL_COUNT];
 for(uint8_t id=0;id<5;++id)for(auto art:expressions){
  display.clearDisplay();setSpriteOrigin(60,70);drawColoredKitsune(art,0,0,static_cast<FurPaletteId>(id));
  buildKitsuneRoles(resolveKitsuneFrame(art),roles);const auto &colors=furPalette(static_cast<FurPaletteId>(id));
  const uint16_t expected[]={COLOR_BACKGROUND,colors.outline,colors.primary,colors.accent,colors.detail};
  bool present[5]={};
  for(int y=0;y<96;y++)for(int x=0;x<96;x++){
   uint8_t role=roles[(y/2)*48+x/2];assert(role<=4);present[role]=true;
   assert(display.getBuffer()[(70+y)*240+60+x]==expected[role]);
  }
  // A blink may deliberately omit white eye details, but must retain the body.
  assert(present[0] && present[1] && present[2]);
  assert(roles[0]==0);
 }
 display.display();assert(windows>0 && windows<225);
 sleepDisplay();assert(backlight==LOW && held && panelSleeping && !displayOn);
 windows=0;display.display();assert(windows==0 && backlight==LOW);
 initializeDisplayBus();initializeDisplay();assert(!held && backlight==LOW);
 setSpriteOrigin(60,70);drawColoredKitsune(KITSUNE_SLEEP,0,0,FurPaletteId::ORANGE);
 display.display();assert(windows==225 && backlight==HIGH);
 puts("PASS: exact SPI pins/no MISO, RAM-only clear, unchanged-frame suppression, single changed tile, full resync, runtime color and exact 2x native 48x48 role scaling, backlight/sleep/hold/wake sequence.");
}

#endif
