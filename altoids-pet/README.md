# FairWeather Friend

FairWeather Friend is a pocket-sized virtual field-research buddy that logs
weather observations when you open it. The friendly kitsune is a self-sufficient
little meteorologist: it needs no feeding, cleaning, care, or friendship grinding.
There is no hunger, health, neglect, punishment, or death. Progress comes from
observing weather, setting lifetime records, discovering weather types, and
unlocking cosmetic field gear. Missed weather is intentionally missed.

## Hardware and wiring

The display target is a **Seeed Studio XIAO ESP32-S3** with a **1.54-inch
240×240 ST7789 SPI TFT**, three momentary buttons, and the existing optional
passive piezo. The SH1107/I2C display is replaced; power/charging is unchanged.

| Connection | XIAO pin | GPIO | Wiring |
| --- | --- | --- | --- |
| Button A | D0 | 1 | Button to GND |
| Button B / wake | D1 | 2 | Button to GND |
| Button C | D2 | 3 | Button to GND |
| Optional passive piezo | D3 | 4 | D3 → 220 Ω → piezo +; piezo − → GND |
| TFT RES/reset | D4 | 5 | RES |
| Future Hall sensor | D5 | 6 | Reserved, unused |
| TFT DC | D6 | 43 | DC |
| TFT CS | D7 | 44 | CS |
| TFT SCL/clock | D8 | 7 | SCL (SPI SCK) |
| TFT BLK/backlight | D9 | 8 | BLK, active HIGH control |
| TFT SDA/data | D10 | 9 | SDA (SPI MOSI) |
| TFT supply | 3V3 | — | VCC |
| Common ground | GND | — | TFT/buttons/piezo GND |

The module's SDA/SCL labels mean **SPI**, not I2C. MISO is unused; D9 is
explicitly the backlight output rather than the board's default SPI MISO.
Buttons remain active LOW with pull-ups. See [Seeed's board guide](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/)
for pinout and programming recovery. Battery voltage monitoring is not implemented.
The XIAO hardware handles charging; firmware does not change it.

Display pins/dimensions/rotation/SPI speed live in `firmware/altoids_pet/hardware.h`.
`TFT_ROTATION` defaults to 0; change only this constant to rotate the panel.

Each button produces one immediate press-down event and stays latched through
holding and release bounce. It rearms only after HIGH is observed continuously
for 120 ms. Release itself never advances a page or repeats an action. Buttons
are debounced independently without blocking delays. A button held at boot/wake
must first be released, so holding B to wake does not also trigger a pet action.

## Arduino setup, configuration, and upload

Use the working board options and installed library versions. Select
**XIAO_ESP32S3** in **esp32 by Espressif Systems**. The current full build was
verified with Arduino-ESP32 **3.3.12**.

Required libraries:

- Adafruit GFX Library and **Adafruit ST7735 and ST7789 Library** (provides
  `Adafruit_ST7789.h`), with Adafruit BusIO dependencies. SH110X is no longer needed.
- ArduinoJson (current installed version 7.4.3, `JsonDocument` API).
- Core-provided SPI, WiFi, WiFiClientSecure, HTTPClient, Preferences, LEDC,
  time/NTP, and RTC/deep-sleep APIs. The ST7789 driver is the only new display dependency.

From `altoids-pet/`, if the private configuration does not already exist:

```sh
cp firmware/altoids_pet/config.example.h firmware/altoids_pet/config.h
```

Edit only `config.h` with Wi-Fi credentials, latitude/longitude, and desired
existing settings. It is gitignored. Never copy real credentials into tracked
files. Defaults retain Central time with DST (`CST6CDT,M3.2.0/2,M11.1.0/2`),
07:00–22:00 awake hours, 30-second inactivity sleep, a three-hour weather-cache
threshold, and 250 ms home animation frames.

Open `firmware/altoids_pet/altoids_pet.ino`, select the board and USB port, Verify,
then Upload. The folder and `.ino` name match Arduino's requirement. Use Serial
Monitor at **115200 baud**. If upload fails, use the Seeed guide's BOOT/reset
procedure. Keep **Erase All Flash Before Sketch Upload: Disabled** and preserve
the existing partition layout.

**Normal firmware uploads preserve NVS buddy progress.** Complete battery
removal/depletion also preserves it. Full flash erase, erasing NVS, factory reset,
or incompatible partition changes can destroy it. Export Buddy before risky
updates, partition changes, or device replacement. No startup or migration code
clears NVS. The original `../testsketch` remains a historical prototype and is
not the current firmware entry point.

## Project modules

```text
altoids-pet/
├── firmware/altoids_pet/
│   ├── altoids_pet.ino
│   ├── display.cpp / display.h
│   ├── pet.cpp / pet.h
│   ├── sprites.cpp / sprites.h
│   ├── gear_sprites.cpp / gear_sprites.h
│   ├── gear_overlay.cpp / gear_overlay.h
│   ├── weather.cpp / weather.h
│   ├── power.cpp / power.h
│   ├── timer.cpp / timer.h
│   ├── sound.cpp / sound.h
│   ├── journal.cpp / journal.h
│   ├── journal_ui.cpp / journal_ui.h
│   ├── gear.cpp / gear.h
│   ├── save.cpp / save.h
│   ├── config.example.h
│   ├── hardware.h
│   ├── palette.h
│   ├── display_surface.cpp/.h
│   └── config.h                  # local, gitignored
├── tests/journal/                # save/observation host checks
├── tests/journal_ui/             # screen/control host checks
├── tests/gear_overlay/           # bitmap/composition host checks
├── tests/buttons/                # press/release debounce host checks
├── tests/display/                # real GFX + mocked SPI/TFT transport and previews
├── hardware/
├── enclosure/
├── assets/sprites/
├── docs/
├── README.md
├── LICENSE
└── .gitignore
```

- `display`: 240×240 screen layouts and existing button routing.
- `display_surface`: RGB565 canvas, changed-tile SPI updates, backlight and TFT sleep.
- `hardware`: exact TFT wiring, dimensions, rotation, and SPI speed.
- `palette`: fixed UI colors plus runtime fur/gear colors, independent of saves.
- `pet`: temporary visual moods, B reaction, sleep schedule, and non-blocking
  idle/weather animation state machines. No permanent care/progression stats.
- `sprites`: PROGMEM kitsune artwork and bitmap composition.
- `gear_sprites`: independent PROGMEM cosmetic foreground/mask bitmaps.
- `gear_overlay`: read-only equipment composition over the current pet frame.
- `weather`: existing Wi-Fi/NTP/timezone, live fetch, moon/sun calculations, and
  RTC weather cache. A hook at successful live-fetch completion records weather.
- `power`: existing inactivity sleep, debounce, RTC B pull-up, and wake handling.
- `timer`: presets/countdown/DONE and independent RTC timer recovery.
- `sound`: optional brief piezo cues; no added blocking delays.
- `journal`: observation rules, runtime journal view, checkpoints, and USB commands.
- `journal_ui`: Journal/Records/Gear screens, paging, and equipment selection.
- `gear`: stable cosmetic IDs, a central unlock-rules table, and equipment hook.
- `save`: versioned byte encoding, NVS slots, old-birthday migration, JSON validation.

## What counts as an observation

An observation is created only after the existing **successful live Open-Meteo
fetch during boot/wake**, with a valid system clock, numeric temperature, and
supported weather category. It carries the UTC timestamp, temperature in tenths
Fahrenheit, raw weather code, and mapped category. Display temperature formatting
and the fetch URL are unchanged. Missing/invalid measurements are not rewarded.

Each accepted observation updates:

- Total observations and unique **Central local calendar days** observed.
- Latest timestamp, date, temperature, and weather category/code.
- Counts/discoveries for clear, mainly clear, partly cloudy, cloudy, rain, storm,
  snow, and fog.
- Highest/lowest temperatures and the first timestamp each record was set.
- Cosmetic gear unlocks, followed by a verified NVS checkpoint.

Equal temperatures keep the earlier record date. Observations must advance past
the previous timestamp by at least **60 seconds**. This prevents rapid cold
reboot farming and rejects backward-clock observations. Unique days increment
when an accepted observation reaches another Central calendar date; DST uses
the same timezone configuration as the clock. There is no historical backfill.
The journal stores aggregates and the latest observation, not an unbounded list
of every sample.

**Cached weather is display data, not a new observation.** B wakes continue to
show RTC weather immediately and leave Wi-Fi off when the existing cache is
usable. The existing three-hour threshold and cold-boot fetch policy are unchanged.
Pressing B, using menus, and completing timers do not award journal progress.
Pet sleep hours affect visuals/sound, not whether a successful live fetch can
be recorded.

If NTP/measurement validation fails, that fetch is not backdated later. The
existing fetch can still display weather; the journal logs why it skipped progress.

## Permanent saves and RTC cache

`BuddySaveData` in `save.h` is **saveVersion 2**. It contains the research start
(`createdAt`, exported as `researchBeganAt`), observation/day totals, latest
observation, dated high/low records, eight category counters, discovery flags,
unlocked gear flags, and six equipped slot IDs. A zero start timestamp means the
existing clock has not become valid yet. It is captured once and never reset
by observations, sleep, reboot, or firmware upload. A confirmed Import Buddy is
an explicit replacement with the imported buddy's own start date.

Permanent progress lives in **NVS namespace `fwf-buddy`**, keys `save0`/`save1`.
Each record has a stable header (magic, version, length, generation, checksum)
and an explicit 168-byte little-endian payload. Native structs, padding, mood,
Wi-Fi settings, and RTC caches are not serialized. Writes alternate slots and
are read back/verified; load selects the newest valid slot and can fall back
from a checksum-damaged slot.

Version 1 buddy records (148-byte payloads) are verified and migrated explicitly:
all progress/unlock fields remain intact, the old equipped item goes into its
matching slot, and other slots start at NONE. The newest valid generation wins
across v1/v2 records. Migration queues a normal checkpoint, which writes v2 to
the alternate NVS key and retains the old record as a fallback. Failed writes
leave migration pending. Unknown/newer versions on either key block all writes.

The old `altoids-pet` namespace remains untouched. Migration reads the supported
old `state0`/`state1` or original `state` record and copies **only the birthday**;
the separate old `birthday` key is authoritative. Friendship, B interaction
counts, and cooldowns are ignored. Migration and the next observation can share
one checkpoint. No old weather observations are invented from interactions.

Unknown versions, unrecognized/truncated layouts, and unreadable storage disable
progress writes/imports and produce clear Serial errors. Existing data is
preserved. An invalid checksum can use another valid supported slot. If neither
slot is valid, the loader logs that and tries the supported legacy birthday or
initializes a new journal; there is no silent full-flash reset. Future versions
extend the decoder/migration dispatch in `save.cpp`, rather than reinterpreting
old payloads. New event categories, wind/humidity/pressure records, alerts, moon
collections, and more gear can be added with explicit version migrations.

NVS writes occur only for new/migrated save initialization, first valid creation
time, accepted live observations, changed equipment, and confirmed import.
Unchanged loops, cached wakes, animations, and B reactions do not write flash.
Normal pre-sleep checkpointing writes only pending changes. A failed save retains
the previous NVS checkpoint, retries at most once per minute while awake, and
gets another attempt at the existing sleep checkpoint. Pending RAM changes can
be lost if power is removed before a failed write is recovered; this is logged.
Export is deferred until pending progress has been saved successfully.

**RTC memory** retains only the existing weather/sunrise/sunset/cache freshness
and timer runtime records. The permanent journal is loaded from NVS on every
boot, including deep-sleep wake. Battery death loses RTC weather/timer cache,
not the committed journal.

## Cosmetic field gear

| Stable ID | Item | Unlock |
| --- | --- | --- |
| 0 | NONE | No item equipped |
| 1 | FIELD_CAP | First observation |
| 2 | SUNGLASSES | First clear or mainly-clear observation |
| 3 | UMBRELLA | First rain observation |
| 4 | RAINCOAT | Ten rain observations |
| 5 | WINTER_SCARF | First snow observation |
| 6 | WINTER_COAT | Five snow observations |
| 7 | BOOTS | First observation below 32.0°F |

Gear is cosmetic, with no bonuses or care requirements. Unlock flags use bit
`ID - 1`; discovery flags use bits 0–7 in the category order above. Equipment
starts at NONE in each of HEAD, FACE, NECK, BODY, FEET and PROP.
`equipGear(slot, gear)` rejects locked, invalid, or incompatible items and
checkpoints only changed selections. Equipping replaces only that slot.

| Slot | Compatible gear |
| --- | --- |
| HEAD | FIELD_CAP |
| FACE | SUNGLASSES |
| NECK | WINTER_SCARF |
| BODY | RAINCOAT, WINTER_COAT |
| FEET | BOOTS |
| PROP | UMBRELLA |

All equipped items draw over the current kitsune on HOME, FOCUS and DONE.
Existing weather umbrella/scarf reactions continue independently.

### Cosmetic overlay layer

`gear_sprites.cpp` contains seven separate 24×24 foreground bitmaps, with local
occlusion masks for the cap, umbrella, coats, scarf, and boots. Each bitmap is
72 MSB-first bytes in PROGMEM, with empty pixels outside the accessory. They
contain no duplicate pet frames. Sunglasses are outline-only and leave the
existing eye interiors visible for blink/look expressions.

`gear_overlay.cpp` reads `getBuddySave().equippedSlots` without mutating progress.
NONE, invalid IDs, or unavailable/protected saves draw nothing. The layer reuses
the existing 2× bitmap expansion and three-row torso-crouch transform. Masks
paint only local covered pixels black in RAM, followed by white accessory pixels;
transparent pixels do not erase the background. No new framebuffer or display
transfer is introduced.

Layer order is weather background → base kitsune → BODY → FEET → NECK → HEAD → FACE → PROP → weather
reaction accessories → heart/focus book. Gear follows the already-computed
B hop, idle bounce, snow shiver, and DONE offsets. Boots also follow the bounce
sprite's lifted paws. The hat/glasses remain rigid during the ear-tip twitch;
the held umbrella remains rigid during a body crouch. Gear stays visible on
sleeping pets without adding any awake animation.

An active umbrella/scarf weather reaction temporarily supplies that same
equipped item, preventing duplicate umbrellas/scarves. Other equipment remains
visible beneath the weather accessory. The original weather reaction graphics,
timing and cancellation behavior are unchanged. Hearts and the focus book
remain in front, including when an umbrella is equipped.

To replace an accessory, edit its named foreground/mask arrays in
`gear_sprites.cpp`. Keep the base 24×24 coordinates, three bytes per row, and
mask silhouette aligned. The umbrella uses a separate held-item offset; all
other items use the base sprite anchor. No pet-state or progression changes are
needed. Unlock rules and equipment NVS storage remain in their existing modules.

The menu is WEATHER, TIMER, JOURNAL, RECORDS, GEAR, SETTINGS. WEATHER and TIMER
retain their first two positions and existing controls. The six entries fit
on one screen. SETTINGS remains a selection-log placeholder; future action IDs
cover EXPORT BUDDY, IMPORT BUDDY, LOCATION, UNITS, SOUND, DISPLAY, ABOUT.

## Journal, Records, and Gear screens

From HOME, A opens the menu, A advances its selection, and B opens the selected
screen. C from these three screens returns to the menu at the same selection;
C from the menu retains its existing return-home behavior.

- **JOURNAL:** A cycles four pages: total observations/days/discovered types and
  FIELD RESEARCH BEGAN date; latest observation's Central date/time, temperature,
  category and weather code; clear/mainly-clear/partly-cloudy/cloudy counts; and
  rain/storm/snow/fog counts. Filled dots mark discovered categories; hollow dots
  mark undiscovered ones. B has no action.
- **RECORDS:** Shows highest/lowest temperatures to a tenth Fahrenheit with their
  Central dates/times. A and B have no action. With no observations, it shows an
  explicit waiting message rather than zero-temperature records.
- **GEAR:** Opens a six-slot overview showing the equipped item in each slot.
  A cycles slots; B opens the selected slot. Within a slot, A cycles NONE and
  compatible unlocked items; B equips the highlighted choice. NONE removes only
  that slot. C returns to the overview, then C returns to the menu. SAVE FAILED
  leaves equipment intact. Selecting an already-equipped item does not write
  NVS again. With no compatible unlocks, the slot offers only NONE.

Viewing pages does not award observations, fetch weather, write progress, or
play new sounds. Equipment selection uses the existing verified NVS hook and
survives reboot/power loss. Protected/unavailable storage displays JOURNAL
UNAVAILABLE / SAVE PRESERVED and disables equipment changes. Unknown creation
time displays WAITING FOR TIME.

Each render composes the existing framebuffer and calls `display()` once. The
partial-update driver is unchanged. Static screens are not animated or redrawn
continuously; a 250 ms check refreshes them only if the saved-data checksum or
availability changes, such as after USB import. Normal inactivity sleep and B
wake still apply to these screens. Wake returns to the existing home/timer flow.

## USB Export Buddy / Import Buddy foundation

Use USB Serial at **115200 baud**, with newline-terminated commands. Export/import
serialize all permanent fields, including a canonical-payload FNV-1a checksum
written as eight hexadecimal characters. It detects accidental corruption; it
is not an authentication mechanism. JSON uses tenths Fahrenheit regardless of
future display units and contains no Wi-Fi credentials or weather cache.

Version 2 JSON replaces `equippedGear` with all six numeric slot IDs:

```json
"equippedSlots": {"HEAD": 1, "FACE": 2, "NECK": 5, "BODY": 4, "FEET": 7, "PROP": 3}
```

IDs remain unchanged; 0 means NONE. Import requires all six slots, compatible
unlocked items, and a matching checksum. Version 1 JSON backups are also accepted:
their original v1 checksum is verified before migrating the item and upgrading
to v2. Use the confirmation checksum printed by staging, which describes the
migrated v2 buddy. Newer/unknown versions are rejected without writes.

```text
EXPORT_BUDDY
IMPORT_BUDDY { ...complete one-line exported JSON... }
CONFIRM_IMPORT 1234abcd
CANCEL_IMPORT
```

1. Send `EXPORT_BUDDY`. Copy **only the JSON object** into `buddy.json`, excluding
   startup/diagnostic logs. Pretty-printed output is human-readable.
2. Import using `IMPORT_BUDDY ` followed by the complete JSON on **one line**.
   Required fields/types, version, counters, dates, temperature records, flags,
   equipment, and checksum must validate. Failed validation writes nothing.
3. Successful staging prints an exact `CONFIRM_IMPORT <checksum>` command.
   Send that command within **60 seconds** to explicitly replace the current
   buddy. Staging alone writes nothing. Cancel, timeout, sleep, or reset discards
   the staged import. Failed NVS confirmation keeps the current runtime buddy.
4. Export again and compare the JSON to verify the restored buddy.

To compact an exported JSON file for the Serial Monitor:

```sh
python3 -c 'import json,sys; print("IMPORT_BUDDY "+json.dumps(json.load(sys.stdin),separators=(",",":")))' < buddy.json
```

`exportBuddy()`, `importBuddy()`, `confirmBuddyImport()`, serialization, and
validation functions are available for a future settings UI. Commands use a
fixed 4096-character line limit and consume at most 64 received bytes per loop;
there are no blocking Serial line reads. **Normal 30-second sleep still applies**
during transfer; complete commands/confirmation promptly or use an already
running focus timer to keep the device awake. Serial activity does not change
power or button policy. No save-reset command is provided.

## Preserved display, companion, timer, and sound behavior

The ST7789 uses hardware SPI: `SPI.begin(D8, -1, D10, D7)`,
`init(240, 240, SPI_MODE0)`, rotation 0, and a 40 MHz transfer clock. The driver
handles the controller's 240×240 address offset. Rotation is isolated in
`hardware.h`. The firmware cannot identify/check panel presence through this
write-only connection; wiring and orientation require physical verification.

The old SH1107 page-transfer logic is replaced by a **single RGB565 canvas**.
`clearDisplay()` clears RAM only. `display()` hashes the final 16×16 tiles and
transfers only tiles that changed since the last completed frame. A clock update
therefore leaves unchanged pet/environment tiles alone; repeated static frames
send nothing. Initial boot/wake resynchronizes the full final image. Navigation
can change most tiles, but never transfers an intermediate blank image. This
uses 115,200 bytes of canvas heap plus 900 bytes of tile hashes; a second RGB
framebuffer is avoided. SPI transfers are synchronous, without new animation
delays. Scheduler cadence and button timing remain unchanged. Actual panel
tearing/transfer speed and heap headroom during online fetch need hardware tests.

Backlight is driven LOW before panel initialization and enabled only after the
first restored HOME/FOCUS/DONE frame is transferred. Before deep sleep, the
existing sleep path calls `sleepDisplay()`: D9 goes LOW and is held LOW through
deep sleep, then the TFT receives display-off and sleep-in commands. On B wake,
normal setup releases the hold, initializes the TFT, restores cached state, and
lights the completed frame. No display path starts Wi-Fi or changes wake sources.

The home layout places a larger clock at the top, environment around the pet,
a ground baseline, then temperature and weather labels. Menus use 28-pixel row
spacing. Journal/Records/Gear preserve pages, data and controls with larger text;
long lifetime totals still fit exactly. Focus shows a large countdown above the
reading pet. UI coordinates are physical TFT pixels. Only pet/accessory
composition maps the existing local geometry at 3/2, turning the existing 2×
masks into **exact 3× art (72×72)** without interpolation or timing changes.

`palette.h` defines a dark background, warm light text, and muted warm/cool
accents. `petPalette.fur` and `petPalette.gear[GearId]` are runtime RGB565 colors:
change those values to recolor the same 1-bit masks. They are rendering-only,
with no new menus, save fields, unlock rules, or duplicated artwork. Gear layer
order, clothing masks, eye visibility and animated attachment offsets are intact.
Battery status is not added.

B on awake HOME keeps its 700 ms hop/heart, temporary EXCITED then HAPPY mood,
and optional sound. It has no lifetime counter or friendship reward. Night-time
sleep remains authoritative. Occasional blink/double blink, looks, bounce, ear
twitch, and weather reactions remain non-blocking at the existing frame cadence.

The original PROGMEM kitsune assets remain in `sprites.cpp`: nine base sprites
plus LOOK_UP. Each is **24×24, 72 bytes**, three MSB-first bytes per row, white
bits on a transparent background. They render at crisp 3× scale on the TFT. To replace an
expression, edit its array initializer while preserving the name/dimensions;
row comments are illustrative only. Keep ear/neck/feet alignment for existing
transforms/accessories. Umbrella, scarf, book and hearts remain separate overlays.

TIMER still offers 5/10/15/25 minutes: A selects, B starts, C backs out/cancels;
C on DONE returns home. Running focus prevents normal inactivity sleep. Countdown,
book, DONE celebration, and RTC resume/deadline behavior are unchanged. Full
power loss clears the timer; it is not permanent buddy data.

Optional piezo settings in private `config.h`:

```cpp
#define PIEZO_PIN D3
#define SOUND_ENABLED true
#define SOUND_STARTUP_CHIRP false
```

Old configs still use the sound module's defaults. A successful awake B reaction
uses its rate-limited 55 ms chirp. Timer completion retains its two short tones,
including at night. Other cues remain silent while the pet sleeps. The installed
Arduino-ESP32 3.x LEDC API drives sound; PWM shuts down before sleep. No sound
preference/UI or new library is added by this refactor.

## Validation and device checklist

Host tests use the real installed ArduinoJson with temporary NVS/Serial stubs.
They cover initialization, migration from both legacy layouts, immutable birthday,
reboot/power retention, observations/60-second guard, Central dates, records/ties,
category counts/discovery, gear thresholds/equipment, write retries/slot fallback,
future-version protection, JSON roundtrip/rejection, import confirmation/cancel/
expiry, and bounded Serial input. Run from the repository root (replace the
ArduinoJson include path with your installed location). On macOS with Command
Line Tools, explicitly select its C++ headers; on Linux omit the `-isystem` line:

```sh
c++ -std=c++17 \
  -isystem "$(xcrun --show-sdk-path)/usr/include/c++/v1" \
  -Ialtoids-pet/tests/journal/stubs -Ialtoids-pet/firmware/altoids_pet \
  -I"$HOME/Documents/Arduino/libraries/ArduinoJson/src" \
  altoids-pet/tests/journal/test_journal.cpp -o /tmp/fwf-journal-tests
/tmp/fwf-journal-tests
```

The screen tests use the same NVS/JSON implementation and a display stub that
checks every text/circle stays inside 240×240. They cover all Journal pages,
record/empty displays, slot browsing and compatible unlocked choices, equip/unequip, duplicate-write
avoidance, save failures, NVS restoration, refresh after import, protected storage,
navigation, unchanged-frame suppression, and one framebuffer update per render:

```sh
c++ -std=c++17 \
  -isystem "$(xcrun --show-sdk-path)/usr/include/c++/v1" \
  -Ialtoids-pet/tests/journal_ui/stubs -Ialtoids-pet/tests/journal/stubs \
  -Ialtoids-pet/firmware/altoids_pet \
  -I"$HOME/Documents/Arduino/libraries/ArduinoJson/src" \
  altoids-pet/tests/journal_ui/test_journal_ui.cpp -o /tmp/fwf-journal-ui-tests
/tmp/fwf-journal-ui-tests
```

Gear-overlay tests use a pixel-accurate monochrome bitmap stub plus the actual
pet, sprite, and overlay code. They verify transparent/masked pixels for every
item, screen bounds, no-op IDs/protected saves, sleep/focus/DONE poses, eye
visibility, ear/bounce/shiver/crouch alignment, duplicate-weather-item suppression,
stale-weather recovery, and unchanged animation frame gating:

```sh
c++ -std=c++17 \
  -isystem "$(xcrun --show-sdk-path)/usr/include/c++/v1" \
  -Ialtoids-pet/tests/gear_overlay/stubs -Ialtoids-pet/firmware/altoids_pet \
  altoids-pet/tests/gear_overlay/test_gear_overlay.cpp -o /tmp/fwf-gear-overlay-tests
/tmp/fwf-gear-overlay-tests
```

Button tests simulate press/release bounce, long holds, independent inputs,
held-at-wake B, activity timestamps, millis rollover, and 100 complete cycles:

```sh
c++ -std=c++17 \
  -isystem "$(xcrun --show-sdk-path)/usr/include/c++/v1" \
  -Ialtoids-pet/tests/buttons/stubs -Ialtoids-pet/firmware/altoids_pet \
  altoids-pet/tests/buttons/test_buttons.cpp -o /tmp/fwf-buttons-tests
/tmp/fwf-buttons-tests
```

The ST7789 transport tests compile the actual Adafruit GFX canvas/bitmap/text
implementation with a mocked SPI panel. They check exact pin mapping (including
no MISO), RAM-only clearing, unchanged-frame suppression, changed-tile updates,
full resynchronization, pixel-perfect 3× art, runtime colors, and backlight/hold/
sleep/wake sequencing. Layout tests exercise actual HOME/weather/menu/timer and
Journal/Records/Gear rendering, all weather states, simultaneous gear, idle poses,
sleep, and minute-only partial updates. They write PPM previews to `/tmp`.

```sh
for name in display layout; do
  c++ -std=c++17 -DARDUINO=100 \
    -isystem "$(xcrun --show-sdk-path)/usr/include/c++/v1" \
    -Ialtoids-pet/tests/display/stubs -Ialtoids-pet/firmware/altoids_pet \
    -I"$HOME/Documents/Arduino/libraries/Adafruit_GFX_Library" \
    altoids-pet/tests/display/test_${name}.cpp \
    "$HOME/Documents/Arduino/libraries/Adafruit_GFX_Library/Adafruit_GFX.cpp" \
    -o /tmp/fwf-${name}-tests
  /tmp/fwf-${name}-tests
done
```

The display migration leaves save/version/migration/JSON, networking/time/cache,
weather observations, timer, sound, unlock rules, button debounce and wake-source
logic unchanged. Only rendering and the display power-off hook change. The full
XIAO ESP32-S3 build is verified. No upload or hardware test is performed automatically.

On the physical device:

- Wire the exact SPI map above; leave D5 disconnected/reserved. Check rotation,
  all four edges, colors, and readability. TFT BLK must be controlled by D9.
- On cold boot and B wake, verify the first illuminated frame is complete, with
  no white/black flash. Leave idle for 30 seconds: backlight must go fully off,
  B must wake once, cached weather/time must restore without unnecessary Wi-Fi.
- Watch several minute changes, blink/double blink, looks, ear twitch, B hops,
  bounce, night sleep, and every weather animation. Check no trails/black flashes.
- Equip all six slots. Verify 3× pixel edges, masks, eyes, ear/tail/feet alignment,
  crouch/shiver/bounce movement, umbrella/scarf replacement and foreground hearts.
- Browse all menus and Journal pages, Records and Gear slots/items. Check margins,
  longest labels, lifetime counts, footer readability, one action per A cycle,
  unchanged B equip/NONE and C back navigation.
- Run each timer preset through focus/DONE. Check large countdown, book/gear
  overlap, night sleeping pose, happy completion, sound, and sleep inhibition.
- Export Buddy before/after upload with erase disabled. Confirm progress, version
  2, all six equipped slots and JSON confirmation behavior are unchanged.
- Test an actual online weather/NTP fetch and subsequent cached wakes; check
  Serial for resets/allocation failures while the larger canvas is allocated.


- Open JOURNAL, cycle all four pages, and compare totals, dates, latest weather,
  counts and discovery dots with exported JSON. Confirm B changes nothing and
  C returns to the same menu entry.
- Press/hold/release A in the menu, JOURNAL, GEAR and timer setup. Each cycle
  should advance exactly once on press-down, with no repeat while held or when
  released. Allow at least 120 ms of release before the next press. Check one B
  action and one C return per cycle; hold B while waking, then release and press
  again to verify only the fresh press interacts with the pet.
- Open RECORDS and check positive/negative temperatures and dates against JSON.
  Check empty-journal messages using the host tests or a deliberately confirmed
  test import after exporting your real buddy.
- Export Buddy before updating, then upload with the existing partition scheme
  and flash erase disabled. Verify the v1 equipped item moves to its correct
  slot, and Journal/Records/unlocks/start date remain unchanged.
- Browse all six Gear slots. Equip multiple unlocked items; switch BODY between
  coats and verify other slots remain equipped. Set one slot to NONE and verify
  only it clears. Check A cycles once per press and C backs out one level.
  Deep-sleep/wake and unplug/reconnect; verify every slot persists. Export and
  confirmed-import a backup and compare all six slots and existing progress.
- Return HOME after equipping each unlocked item: check its outline, mask and
  face/ear/tail alignment. Check blink/look visibility with glasses, hops and
  lifted boots, snow shiver, storm crouch, and sleeping appearance. With umbrella
  or scarf equipped, verify the matching weather reaction shows a single item
  and returns to the equipped art afterward. Confirm B hearts and the focus book
  remain in front, DONE celebration still works, and no trails/black flashes occur.
- Leave each screen idle: no repeated refresh or flash should occur. Check
  normal 30-second sleep/B wake, weather/timer controls, sound and home animations.
  When practical, import a valid different buddy while a screen is open and verify
  the displayed values refresh once. Restore your own exported buddy afterward.

1. **Migration/cold boot:** Note the old birthday before updating. Upload with
   full-flash erase disabled. Check the migration/NVS log and `researchBeganAt`
   export; friendship/interactions must not become observations. The first live
   fetch should log one observation and unlock the cap plus applicable gear.
2. **Cached sleep/wake:** Wait for normal 30-second sleep, wake with B, and verify
   clock/cache restoration with Wi-Fi off. Export totals before/after: unchanged
   without a new live fetch. Repeat several wakes; check no black flashes.
3. **Full power loss:** Note/export the committed journal, remove USB and battery,
   reconnect, and verify it loads from NVS with the same start/records/gear. A
   successful cold-boot live fetch may then add one observation normally.
4. **Live fetch/repeated opening:** Verify a successful live fetch increments
   totals/category count once. Rapid cold reboot within 60 seconds may still
   fetch (existing Wi-Fi policy) but must log the guard and award no extra progress.
   Failed network/fetch/time validation must not award observations.
5. **Days/records:** Observe on both sides of Central midnight; same-day fetches
   must not add days. Check dated high/low changes and unchanged tie dates in
   JSON. A long gap adds only the newly observed day, with no backfill.
6. **Discovery/gear:** Check first clear/rain/snow and below-freezing observations
   unlock the documented flags; later eligible live rain/snow observations meet
   10/5 thresholds. Existing weather accessory animations must remain intact.
7. **Reflash:** Export, upload the same firmware with unchanged partition and
   erase settings, then verify the same buddy date/progress is loaded. Account
   for any new accepted live fetch. Do not erase flash to test routine updates.
8. **USB transfer:** Export valid JSON. Stage an import and verify no write before
   confirmation; confirm its checksum and export again. Try malformed JSON,
   missing fields, bad checksum, newer version, wrong confirmation, cancel, and
   timeout. The current buddy must remain intact on rejection.
9. **Regression:** Exercise all A/B/C controls, pet sleep hours, idle/weather
   reactions, minute changes, sunrise/sunset/moon, all timer controls/countdown/
   warm-reset resume, and sound. B reactions must never change journal totals.

Record updates, gear thresholds and storage failures can be exercised without
waiting for weather using the host tests; no test-only weather overrides or
extra fetch button were added to production firmware.

## Display migration file inventory

Paths below are relative to `altoids-pet/`. This migration changes rendering,
display power control and their tests; permanent save logic and bitmap arrays
are unchanged.

```text
README.md
firmware/altoids_pet/altoids_pet.ino
firmware/altoids_pet/config.example.h
firmware/altoids_pet/config.h             (local, gitignored; hardware include only)
firmware/altoids_pet/display.cpp
firmware/altoids_pet/display.h
firmware/altoids_pet/display_surface.cpp  (new)
firmware/altoids_pet/display_surface.h    (new)
firmware/altoids_pet/hardware.h           (new)
firmware/altoids_pet/palette.h            (new)
firmware/altoids_pet/gear_overlay.cpp
firmware/altoids_pet/journal_ui.cpp
firmware/altoids_pet/pet.cpp
firmware/altoids_pet/power.cpp
firmware/altoids_pet/sprites.h
tests/buttons/test_buttons.cpp
tests/buttons/stubs/Arduino.h
tests/buttons/stubs/Adafruit_SH110X.h     (removed)
tests/buttons/stubs/display_surface.h    (new)
tests/gear_overlay/test_gear_overlay.cpp
tests/gear_overlay/stubs/Arduino.h
tests/gear_overlay/stubs/Adafruit_SH110X.h (removed)
tests/gear_overlay/stubs/display_surface.h (new)
tests/journal_ui/test_journal_ui.cpp
tests/journal_ui/stubs/Adafruit_SH110X.h  (removed)
tests/journal_ui/stubs/display_surface.h  (new)
tests/display/test_display.cpp           (new)
tests/display/test_layout.cpp            (new)
tests/display/stubs/Arduino.h            (new)
tests/display/stubs/Print.h              (new)
tests/display/stubs/SPI.h                (new)
tests/display/stubs/Adafruit_ST7789.h     (new)
tests/display/stubs/Adafruit_I2CDevice.h  (new)
tests/display/stubs/Adafruit_SPIDevice.h  (new)
tests/display/stubs/driver/gpio.h        (new)
```

## License

A license has not yet been selected; see `LICENSE`.

Existing progress regression checks:

1. **Migration/cold boot:** Note the old birthday before updating. Upload with
   full-flash erase disabled. Check the migration/NVS log and `researchBeganAt`
   export; friendship/interactions must not become observations. The first live
   fetch should log one observation and unlock the cap plus applicable gear.
2. **Cached sleep/wake:** Wait for normal 30-second sleep, wake with B, and verify
   clock/cache restoration with Wi-Fi off. Export totals before/after: unchanged
   without a new live fetch. Repeat several wakes; check no black flashes.
3. **Full power loss:** Note/export the committed journal, remove USB and battery,
   reconnect, and verify it loads from NVS with the same start/records/gear. A
   successful cold-boot live fetch may then add one observation normally.
4. **Live fetch/repeated opening:** Verify a successful live fetch increments
   totals/category count once. Rapid cold reboot within 60 seconds may still
   fetch (existing Wi-Fi policy) but must log the guard and award no extra progress.
   Failed network/fetch/time validation must not award observations.
5. **Days/records:** Observe on both sides of Central midnight; same-day fetches
   must not add days. Check dated high/low changes and unchanged tie dates in
   JSON. A long gap adds only the newly observed day, with no backfill.
6. **Discovery/gear:** Check first clear/rain/snow and below-freezing observations
   unlock the documented flags; later eligible live rain/snow observations meet
   10/5 thresholds. Existing weather accessory animations must remain intact.
7. **Reflash:** Export, upload the same firmware with unchanged partition and
   erase settings, then verify the same buddy date/progress is loaded. Account
   for any new accepted live fetch. Do not erase flash to test routine updates.
8. **USB transfer:** Export valid JSON. Stage an import and verify no write before
   confirmation; confirm its checksum and export again. Try malformed JSON,
   missing fields, bad checksum, newer version, wrong confirmation, cancel, and
   timeout. The current buddy must remain intact on rejection.
9. **Regression:** Exercise all A/B/C controls, pet sleep hours, idle/weather
   reactions, minute changes, sunrise/sunset/moon, all timer controls/countdown/
   warm-reset resume, and sound. B reactions must never change journal totals.

Record updates, gear thresholds and storage failures can be exercised without
waiting for weather using the host tests; no test-only weather overrides or
extra fetch button were added to production firmware.

## License

A license has not yet been selected; see `LICENSE`.
