# FairWeather Friend

FairWeather Friend is a pocket-sized virtual field-research buddy that logs
weather observations when you open it. The friendly kitsune is a self-sufficient
little meteorologist is fully grown from the beginning and never evolves. It
needs no feeding, cleaning, care, or friendship grinding.
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
│   ├── kitsune_assets.cpp
│   ├── gear_sprites.cpp / gear_sprites.h
│   ├── gear_overlay.cpp / gear_overlay.h
│   ├── weather.cpp / weather.h
│   ├── weather_cache.cpp
│   ├── weather_observation.cpp / weather_observation.h
│   ├── measurement_units.h
│   ├── power.cpp / power.h
│   ├── timer.cpp / timer.h
│   ├── sound.cpp / sound.h
│   ├── journal.cpp / journal.h
│   ├── journal_ui.cpp / journal_ui.h
│   ├── settings_ui.cpp / settings_ui.h
│   ├── units.h / version.h
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
- `palette`: five stable fur palette IDs, pixel-role RGB565 tables, and gear tints.
- `buddy_setup`: first-run welcome/preview/confirmation, without new care mechanics.
- `pet`: temporary visual moods, B reaction, sleep schedule, and non-blocking
  idle/weather animation state machines. No permanent care/progression stats.
- `sprites`: native role-map/bitmap composition; `kitsune_assets.cpp` holds the kitsune frames.
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
supported weather category. It carries the UTC timestamp, temperature in
thousandths Celsius, raw weather code/category, and optional humidity, wind,
gust, surface pressure and precipitation. Display temperature formatting is
centralized; the request adds optional metric fields. Invalid required
measurements never create observations. Missing optional values remain unset.

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

## First-run Buddy Setup and fur colors

A genuinely new buddy enters WELCOME before the normal home/timer UI. Existing
v1/v2 buddies and legacy birthday migrations skip it. A pending setup resumes
at WELCOME after ordinary sleep/wake or reboot; confirmation is the permanent
checkpoint. The existing Wi-Fi/NTP/weather startup still runs normally, and any
accepted startup observation is retained when the color is confirmed.

- WELCOME: B begins fur selection.
- FUR COLOR: A cycles ORANGE → CREAM → GRAY → BROWN → BLUE → ORANGE.
  The actual shared kitsune renderer previews the highlighted color. C returns
  to WELCOME without writing; B confirms.
- Confirmation immediately writes and verifies the selected ID and
  `setupComplete=true` in NVS. SAVE FAILED stays on selection for B retry.
  Success shows “Ready for field work!” for 1.6 seconds using `millis()`, then
  restores the normal home/timer flow. Confirmed setup cannot be reopened by C.

| Stable palette ID | Color |
| --- | --- |
| 0 | ORANGE (original default) |
| 1 | CREAM |
| 2 | GRAY |
| 3 | BROWN |
| 4 | BLUE |

Selection does not write on A/C, add observations, or change progress. The normal
30-second inactivity sleep and B wake still apply. Unconfirmed highlighted
choices are temporary; after reboot preview starts from the pending saved ID.
There is no fur-settings menu, evolution, naming, gender, XP, or care system.

## Permanent saves and RTC cache

`BuddySaveData` in `save.h` is **saveVersion 5**. It contains the research start
(`createdAt`, exported as `researchBeganAt`), observation/day totals, latest
observation, dated high/low records, eight category counters, discovery flags,
unlocked gear flags, six equipped slot IDs, a stable `furPalette` ID and
`setupComplete` flag, sound ON/OFF, units ID (US=0/METRIC=1), and optional
location coordinates in signed millionths of a degree. A zero start timestamp means the
existing clock has not become valid yet. It is captured once and never reset
by observations, sleep, reboot, or firmware upload. A confirmed Import Buddy is
an explicit replacement with the imported buddy's own start date.

Permanent progress lives in **NVS namespace `fwf-buddy`**, keys `save0`/`save1`.
Each record has a stable header (magic, version, length, generation, checksum)
and an explicit 1104-byte little-endian payload. Native structs, padding, mood,
Wi-Fi settings, and RTC caches are not serialized. Writes alternate slots and
are read back/verified; load selects the newest valid slot and can fall back
from a checksum-damaged slot.

Version 4 records (196-byte payloads) migrate to v6. Their Fahrenheit
high/low/latest values convert to thousandths Celsius; original displayed
Fahrenheit precision and dates are retained. Journal counts, discoveries, gear,
palette, setup completion, timestamps, sound, units and location survive.
Added metrics and lifetime records start unset. Versions 1–3 first receive their
existing defaults/migrations, then the same Celsius conversion. Supported older
JSON checksums are verified using their original Fahrenheit payload encoding.

Version 3 records (176-byte payloads) migrate to version 6, preserving every
progress field, fur palette and setup flag. New settings default to US units,
the `SOUND_ENABLED` config default, and the configured latitude/longitude until
a saved location override exists. Version 2 (168 bytes) and version 1 (148 bytes)
retain their existing palette/setup and single-item-to-slot migrations, then
receive these settings defaults. Existing completed buddies never repeat setup.
The newest valid generation wins across v1/v2/v3/v4/v5/v6 records. Migration queues
an alternate-slot checkpoint, retaining the old record as fallback. Failed
writes leave migration pending. Unknown/newer versions on either key block all
writes and never reset the buddy.

The old `altoids-pet` namespace remains untouched. Migration reads the supported
old `state0`/`state1` or original `state` record and copies **only the birthday**;
the separate old `birthday` key is authoritative. Friendship, B interaction
counts, and cooldowns are ignored. These existing buddies also receive ORANGE
and skip setup. Migration and the next observation can share
one checkpoint. No old weather observations are invented from interactions.

Unknown versions, unrecognized/truncated layouts, and unreadable storage disable
progress writes/imports and produce clear Serial errors. Existing data is
preserved. An invalid checksum can use another valid supported slot. If neither
slot is valid but buddy keys exist, the loader protects the damaged save rather
than initializing a new buddy. A genuinely new buddy requires absent buddy keys
and no legacy buddy. Unsupported/damaged legacy data is also protected. Future versions
extend the decoder/migration dispatch in `save.cpp`, rather than reinterpreting
old payloads. New event categories, alerts, moon
collections, and more gear can be added with explicit version migrations.

NVS writes occur only for new/migrated save initialization, first valid creation
time, accepted live observations, changed equipment, successful first-run
confirmation, changed sound/units/location settings, and confirmed import.
Unchanged loops, cached wakes, animations, and B reactions do not write flash.
Normal pre-sleep checkpointing writes only pending changes. A failed save retains
the previous NVS checkpoint, retries at most once per minute while awake, and
gets another attempt at the existing sleep checkpoint. Pending RAM changes can
be lost if power is removed before a failed write is recovered; this is logged.
Export serializes current validated runtime data without changing it or writing NVS.

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

`gear_sprites.cpp` contains seven separate **48×48** foreground bitmaps and
occlusion masks for the cap, umbrella, coats, scarf, and boots. Each is 288
MSB-first PROGMEM bytes (six bytes per row). They contain no duplicate pet
frames. Sunglasses leave eye interiors visible for blink/look expressions.

`gear_overlay.cpp` reads `getBuddySave().equippedSlots` without mutating progress.
NONE, invalid IDs, or unavailable/protected saves draw nothing. Masks paint only
covered pixels black in RAM, followed by tinted accessory pixels. Transparent
pixels preserve the background. Native pixels render at exact 2× scale; gear
shares the base pet's origin, motion and torso-crouch transform. No additional
framebuffer or display transfer is introduced.

Layer order is weather background → base kitsune → BODY → FEET → NECK → HEAD →
FACE → PROP → weather reaction accessories → heart/focus book. Cap, glasses,
scarf, coats and boots are authored against the larger head, neck, torso and
feet. The umbrella uses a held-item offset of +72/-18 physical pixels from the
base sprite origin. Boots lift two physical pixels with the BOUNCE feet. Gear
follows B hops, idle bounces, snow shivers and DONE offsets. Hat/glasses remain
rigid during ear-tip twitch; the held umbrella remains rigid during crouching.
Gear remains visible while sleeping without introducing awake animations.

Umbrella/scarf weather reactions now use these same separate accessory assets,
with their existing timing and cancellation behavior. An active reaction
supplies the matching equipped item once, preventing duplicate accessories.
Hearts and the focus book remain in front.

To replace an accessory, edit its foreground/mask arrays in `gear_sprites.cpp`,
or its authored geometry in `assets/sprites/generate_kitsune.py`. Preserve the
48×48 canvas, six bytes per row, and alignment of foreground and mask. All
items except the offset umbrella share the base sprite anchor. Unlock rules and
equipment NVS storage remain unchanged.

The menu is WEATHER, TIMER, JOURNAL, RECORDS, GEAR, SETTINGS. WEATHER and TIMER
retain their first two positions and existing controls. The six entries fit
on one screen. SETTINGS opens the six-item menu documented below.

## Rich live weather observations

The Open-Meteo current request now includes `temperature_2m`,
`relative_humidity_2m`, `wind_speed_10m`, `wind_gusts_10m`, `surface_pressure`,
`precipitation` and `weather_code`. Explicit request units are Celsius, km/h
and mm; surface pressure is hPa. Daily sunrise/sunset, weather category mapping,
Central clock handling and the existing fetch/cache schedule remain unchanged.
These are current model conditions, not severe-weather alerts or sensor readings.
See [Open-Meteo's current-condition documentation](https://open-meteo.com/en/docs).
Precipitation records compare the amount returned for the current observation
interval, not daily rainfall or accumulated lifetime rainfall.

Temperature is stored as signed thousandths Celsius (`*TemperatureMilliC`).
Optional fields use signed hundredths of percent, km/h, hPa or mm. Latest
validity bits distinguish missing data from a valid zero. Each new lifetime
record stores its metric integer and UTC timestamp; timestamp zero means unset.
Invalid/null/missing optional fields are excluded individually, while valid
required temperature/weather-code observations still succeed. Supported optional
ranges are humidity 0–100%, wind/gust 0–500 km/h, surface pressure 100–1200 hPa
and precipitation 0–1000 mm. Required temperature range is -130–100 C.

Only successful live fetches submit observations. The existing 60-second guard,
backward-time rejection, Central day counting and observation-triggered writes
remain. Cached wakeups do not submit observations. Greater/lesser record updates
are strict; ties keep the original date. Serial prints one measurement diagnostic
per successful fetch, with unavailable fields labeled, rather than per frame.
The separate `weather_cache.cpp` retains the same RTC save/restore entry points;
it also retains raw Celsius for HOME/WEATHER display after wake.

`measurement_units.h` contains pure temperature conversions; `units.h` centralizes
formatting/conversion for all measurements. US wind uses mph, pressure inHg and
precipitation inches; Metric uses km/h, hPa and mm. No permanent values change
when toggling UNITS. `latestJournalTemperature`, `latestJournalMetric` and
`journalRecord` expose valid canonical readings/records for future gear rules.
Existing boots still require a below-freezing temperature; no gear catalog,
alert collection or unlock threshold changes are added.

Version 5 JSON adds `latestTemperatureMilliC`, `highestTemperatureMilliC`,
`lowestTemperatureMilliC`, `latestMetrics` and `records`. Optional latest values
are JSON null when unavailable. Metric keys encode their scale/unit; each record
has `value` and `timestamp`. Import validates types, ranges, presence, record
bounds, timestamps, paired pressure extrema, existing progress rules and checksum.
Versions 1–4 remain importable with new metrics/records unset. No RTC cache is
exported. The desktop backup utility uses the same commands and accepts v6.

## Settings

SETTINGS contains SOUND, UNITS, LOCATION, EXPORT BUDDY, IMPORT BUDDY and ABOUT.
A cycles, B opens/confirms, C backs out one level. SOUND/UNITS highlights are
only drafts: C discards them; B commits a changed value immediately to verified
NVS. A failed write shows SAVE FAILED and preserves the prior setting. Confirming
an unchanged value does not write. All sound cues honor the stored ON/OFF,
including timer completion; disabling also stops a current cue.

US displays Fahrenheit; METRIC displays Celsius throughout HOME, WEATHER,
Journal and Records. Permanent temperature is Celsius, winds are km/h,
pressure hPa, precipitation mm and humidity percent. UI conversions select
Fahrenheit/mph/inHg/inches or Celsius/km/h/hPa/mm without altering records,
dates or unlock rules. Humidity remains percent in both modes.
The focus/timer screens currently have no temperature display.

LOCATION displays six-decimal latitude/longitude and whether they come from
config or NVS. There is no keyboard/GPS. Send `SET_LOCATION <latitude> <longitude>`
at 115200 baud with a newline or carriage return. Finite latitude [-90,90] and longitude [-180,180]
are required; trailing garbage, missing values and failed NVS writes leave the
old location/cache intact. Coordinates persist as integer microdegrees. A
successful changed location marks runtime/RTC weather and sunrise/sunset stale
without adding observations. The next normal online sync (cold boot or next
wake with stale cache) uses it. Setting a location does not initiate a new Wi-Fi
connection in the current loop. RTC cache entries carry their source coordinates
so an old-location cache cannot be restored after an import or reboot.
Timezone handling remains US Central with the existing DST rule.

EXPORT/IMPORT wait for USB Serial commands. Export shows EXPORT COMPLETE without
changing buddy data or writing NVS. Import requires on-device confirmation as
described below. C discards staged data. Completion offers B/C to return HOME;
palette, gear, sound and units are read from the imported buddy immediately.
ABOUT reads firmware version from `version.h` (`FW_VERSION`), save format version
from `SAVE_VERSION`, and shows board/display and open-source identification.
Settings screens draw once per navigation/value/transfer-status change; idle
polls do not redraw. Normal inactivity sleep and B wake remain unchanged.

## Journal, Records, and Gear screens

From HOME, A opens the menu, A advances its selection, and B opens the selected
screen. C from these three screens returns to the menu at the same selection;
C from the menu retains its existing return-home behavior.

- **JOURNAL:** A cycles five pages: summary and FIELD RESEARCH BEGAN;
  latest conditions (category, temperature, humidity, precipitation);
  latest air/wind (wind, gust, surface pressure, Central date/time);
  clear/mainly-clear/partly-cloudy/cloudy counts; rain/storm/snow/fog counts.
  Unavailable optional values show `--`. Filled dots mark discovered categories; hollow dots
  mark undiscovered ones. B has no action.
- **RECORDS:** A cycles TEMPERATURE (high/low), WIND (strongest sustained/gust),
  ATMOSPHERE (highest humidity, lowest/highest surface pressure), and
  PRECIPITATION (wettest observation). Values use the selected units and show
  Central dates. B has no action; C returns to MENU. Unset records show
  NOT OBSERVED; a genuinely observed zero has a timestamp and displays zero.
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

## Desktop backup utility

Use [`../tools/fairweather_backup.py`](../tools/fairweather_backup.py) to export
and import without Arduino IDE. Python 3.8+ and pyserial are required. See the
[macOS, Windows and Linux setup instructions](../tools/README.md).

```sh
python tools/fairweather_backup.py export
python tools/fairweather_backup.py import backups/your_backup.json
python tools/fairweather_backup.py list-ports
```

Run from the repository root with the documented Python environment active.
Auto-detection supports likely XIAO ports; `--port` overrides it. Exports create
validated, timestamped JSON backups. Imports require typed computer confirmation
and B on the buddy after device validation. Firmware/protocol are unchanged.

## USB Export Buddy / Import Buddy foundation

Use USB Serial at **115200 baud**, with newline, carriage-return, or CRLF-terminated commands. Export/import
serialize all permanent fields, including a canonical-payload FNV-1a checksum
written as eight hexadecimal characters. It detects accidental corruption; it
is not an authentication mechanism. JSON uses canonical metric integers regardless of
selected display units and contains no Wi-Fi credentials or weather cache.

Version 5 JSON contains all six numeric equipment IDs plus the selected fur ID
and setup state, plus settings:

```json
"equippedSlots": {"HEAD": 1, "FACE": 2, "NECK": 5, "BODY": 4, "FEET": 7, "PROP": 3},
"furPalette": 0,
"setupComplete": true,
"soundEnabled": true,
"units": 0,
"locationConfigured": true,
"latitudeMicrodegrees": 32000000,
"longitudeMicrodegrees": -95000000
```

Gear IDs remain unchanged; 0 means NONE. Import requires all slots, compatible
unlocked items, integer fur IDs 0–4, a boolean setup flag, and a matching checksum.
Strings, missing palette fields, invalid IDs and newer versions are rejected
without writes. Version 1/2/3/4/5 backups remain supported: their original byte-format
checksums are verified before upgrading to v6. Versions 3/4/5 retain fur/setup;
versions 1/2 receive ORANGE and setup complete. Versions 1–3 receive settings defaults.
Versions 4/5/6 require valid sound/units/location fields and includes them in the
checksum. Current-version backups also preserve an unfinished setup's false flag.

```text
EXPORT_BUDDY
IMPORT_BUDDY { ...complete one-line exported JSON... }
CANCEL_IMPORT
SET_LOCATION 32.000000 -95.000000
```

1. Open Serial Monitor on the XIAO USB port at 115200 baud; select Newline,
   Carriage return, or Both NL & CR. Open SETTINGS → EXPORT BUDDY, type
   `EXPORT_BUDDY` in Serial Monitor and press Send/Enter. Opening the device
   screen alone waits for this computer request. Copy only the JSON
   object into `buddy.json`, excluding startup/diagnostic logs. Export also works
   as a development Serial command outside the transfer screens.
2. Open SETTINGS → IMPORT BUDDY, then send `IMPORT_BUDDY ` followed by the
   complete JSON on one line. Required types, version, counters, dates, records,
   flags, palette, compatible unlocked gear, settings and checksum must validate.
   Failed validation shows IMPORT FAILED and writes nothing.
3. Valid data shows REPLACE CURRENT BUDDY? Press B within 60 seconds to commit
   and verify NVS; C or `CANCEL_IMPORT` discards it. Timeout, sleep or reset also
   discards staging. `CONFIRM_IMPORT` is recognized but cannot commit over Serial;
   explicit on-device B confirmation is required. A failed NVS commit preserves
   the current runtime buddy.
4. After IMPORT COMPLETE, B/C returns HOME. Export again to verify restoration.

To compact an exported JSON file for the Serial Monitor:

```sh
python3 -c 'import json,sys; print("IMPORT_BUDDY "+json.dumps(json.load(sys.stdin),separators=(",",":")))' < buddy.json
```

`exportBuddy()`, `importBuddy()`, `confirmBuddyImport()`, serialization, and
validation functions now support the settings UI. Commands use a
fixed 8205-byte command limit (8192 JSON bytes plus the import prefix) and consume at most 64 received bytes per loop;
there are no blocking Serial line reads. **Normal 30-second sleep still applies**
during transfer; complete commands/confirmation promptly or use an already
button activity to keep the device awake. Serial activity does not change
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
composition preserves existing local motion amplitudes at 3/2, while native
48×48 art renders at **exact 2× (96×96)**, without interpolation. HOME centers
the sprite canvas at x=72, y=80 above the ground at y=182. Hop and idle offsets
remain unchanged. FOCUS/DONE and setup previews use corresponding centered
origins; the book and hearts remain independent foreground graphics.

`palette.h/.cpp` define five RGB565 palettes. Sprite pixels explicitly encode
roles: **0 transparent, 1 outline, 2 primary fur, 3 light/accent fur, 4 facial/detail**.
`kitsune_assets.cpp` holds eleven native 48×48 PROGMEM role maps, each **1152
bytes**: two pixels per byte, left pixel in the high nibble, 24 bytes per row.
`drawColoredKitsune()` reads roles directly from flash and maps them to the saved
palette. There is no flood-fill inference or duplicated artwork per fur color.
`buildKitsuneRoles()` unpacks a frame when needed; `drawRoleSprite()` can render
an unpacked 48×48 role map with an arbitrary palette for future gear variants.
Current gear remains separate 1-bit masks with its existing tints.

Normal pet rendering reads the saved `furPalette` each frame, so HOME, focus,
DONE, sleeping poses, expressions and confirmed imports use the same selection.
Gear layer order, clothing masks, eye visibility and animated attachment offsets
remain intact. All role passes compose RAM before the existing single display
update. The display driver, pin map and SPI/backlight behavior are unchanged.
Battery status is not added.

B on awake HOME keeps its 700 ms hop/heart, temporary EXCITED then HAPPY mood,
and optional sound. It has no lifetime counter or friendship reward. Night-time
sleep remains authoritative. Occasional blink/double blink, looks, bounce, ear
twitch, and weather reactions remain non-blocking at the existing frame cadence.

Frames are IDLE, BLINK, LOOK_LEFT, LOOK_RIGHT, HAPPY, EXCITED, SLEEPY, SLEEP,
BOUNCE, LOOK_UP and FOCUS. The existing state machine selects these assets;
FOCUS adds a quiet inward-paw reading pose beneath the separate book. Sleep,
blink, mood and reaction precedence remain unchanged.

To replace a frame, edit its named initializer in `kitsune_assets.cpp`, keeping
48×48 dimensions and the role encoding above. Row comments show `.OFAD` roles.
Keep head/neck/feet anchors aligned across frames. Alternatively edit the
standard-library-only authoring script and regenerate both dedicated asset files:

```sh
python3 altoids-pet/assets/sprites/generate_kitsune.py
```

Regeneration overwrites `kitsune_assets.cpp` and `gear_sprites.cpp`, so retain
manual art changes in the generator if using that workflow. No animation or
save logic changes are needed to replace artwork. Umbrella, scarf, book and
hearts remain separate overlays.

TIMER still offers 5/10/15/25 minutes: A selects, B starts, C backs out/cancels;
C on DONE returns home. Running focus prevents normal inactivity sleep. Countdown,
book, DONE celebration, and RTC resume/deadline behavior are unchanged. Full
power loss clears the timer; it is not permanent buddy data.

Optional piezo settings in private `config.h`:

```cpp
#define PIEZO_PIN D3
// Default for new/migrated buddies; the saved SOUND setting takes precedence.
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

Gear-overlay tests use a pixel-accurate RGB565 framebuffer stub plus the actual
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
full resynchronization, pixel-perfect 2× native 48×48 art, runtime colors, and backlight/hold/
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
- Equip all six slots. Verify 2× pixel edges, masks, eyes, ear/tail/feet alignment,
  crouch/shiver/bounce movement, umbrella/scarf replacement and foreground hearts.
- Browse all menus and Journal pages, Records and Gear slots/items. Check margins,
  longest labels, lifetime counts, footer readability, one action per A cycle,
  unchanged B equip/NONE and C back navigation.
- Run each timer preset through focus/DONE. Check large countdown, book/gear
  overlap, night sleeping pose, happy completion, sound, and sleep inhibition.
- Export Buddy before/after upload with erase disabled. Confirm progress and
  all six equipped slots are unchanged; v2 upgrades to v6 with ORANGE and no setup.
- Test an actual online weather/NTP fetch and subsequent cached wakes; check
  Serial for resets/allocation failures while the larger canvas is allocated.


- Open JOURNAL, cycle all five pages, and compare totals, dates, latest weather,
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
   confirmation; press B on the import confirmation screen and export again. Try malformed JSON,
   missing fields, bad checksum, newer version, wrong confirmation, cancel, and
   timeout. The current buddy must remain intact on rejection.
9. **Regression:** Exercise all A/B/C controls, pet sleep hours, idle/weather
   reactions, minute changes, sunrise/sunset/moon, all timer controls/countdown/
   warm-reset resume, and sound. B reactions must never change journal totals.

Record updates, gear thresholds and storage failures can be exercised without
waiting for weather using the host tests; no test-only weather overrides or
extra fetch button were added to production firmware.

## Display migration file inventory

Paths below are relative to `altoids-pet/`. This historical inventory describes
the ST7789 migration. Buddy Setup adds `buddy_setup.cpp/.h` and `palette.cpp`,
and updates save/journal/role rendering without changing the working driver or
pin map.

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

First-run/palette hardware checks:

- On your existing buddy, upload without erasing NVS. Setup must not appear.
  Compare exported journal/records/discoveries/gear/start date to the backup;
  only version and the new ORANGE/setup-complete fields should differ, apart
  from any normally accepted live weather observation.
- On a fresh test buddy, check WELCOME/B, all five A previews, wraparound, C back,
  and B confirmation. Watch the actual kitsune's face/ears/muzzle/tail colors.
  Confirm READY lasts about 1.6 seconds and returns home without a flash.
- Let unfinished setup sleep, B wake, or remove/reconnect power. It must still
  require confirmation, not silently accept an unconfirmed preview.
- Confirm a color, then deep-sleep/wake and completely power-cycle. Setup must
  remain completed and the same color must return. A/C preview causes no NVS
  writes; repeated B after confirmation must not recommit setup.
- Check selected color through every expression, B hop/heart, night sleep,
  weather reactions, timer focus/book and DONE. Equip all slots and check masks,
  eye readability, attachments and unchanged foreground/weather layering.
- Export/import a v5 buddy and verify palette/settings survive. Check old v2/v3/v4 backups
  upgrade to ORANGE without setup. Invalid fur IDs, strings, missing fields and
  bad checksums must leave the current buddy intact.

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
   confirmation; press B on the import confirmation screen and export again. Try malformed JSON,
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

## Settings validation

The Settings host suite uses real save/JSON/journal/UI code with NVS and panel
stubs. It covers draft cancellation, write failures/reboot retention, unit
conversion and record preservation, location parsing/bounds/repeated values,
read-only exports, B-only imports/cancel/expiry/commit failures, protected storage,
text bounds and idle redraw suppression:

```sh
c++ -std=c++17 \
  -isystem "$(xcrun --show-sdk-path)/usr/include/c++/v1" \
  -Ialtoids-pet/tests/journal_ui/stubs -Ialtoids-pet/tests/journal/stubs \
  -Ialtoids-pet/firmware/altoids_pet \
  -I"$HOME/Documents/Arduino/libraries/ArduinoJson/src" \
  altoids-pet/tests/settings/test_settings.cpp -o /tmp/fwf-settings-tests
/tmp/fwf-settings-tests
```

On hardware, export a backup first. Check all A/B/C Settings paths, canceled
sound/units drafts, OFF pet/timer cues and ON restoration across power loss.
Toggle US/METRIC and compare HOME/WEATHER/Journal/Records without changing their
stored records. Test valid/invalid SET_LOCATION, confirm no fabricated observation,
then sleep/wake and verify a new-location live fetch. Restore your original
coordinates afterward. Test export success, malformed/future-version/bad-checksum
imports, valid import cancellation and B-only replacement. Confirm ABOUT text,
unchanged-frame/minute flicker behavior, and normal sleep/wake/cache/timer/gear.
Use the existing partition and erase-disabled settings when uploading yourself.

## Rich weather validation and hardware checks

```sh
c++ -std=c++17 \
  -isystem "$(xcrun --show-sdk-path)/usr/include/c++/v1" \
  -Ialtoids-pet/tests/weather/stubs -Ialtoids-pet/tests/journal/stubs \
  -Ialtoids-pet/firmware/altoids_pet \
  -I"$HOME/Documents/Arduino/libraries/ArduinoJson/src" \
  altoids-pet/tests/weather/test_weather.cpp -o /tmp/fwf-weather-tests
/tmp/fwf-weather-tests
```

The suite tests optional missing/invalid/zero values, every conversion, strict
record improvements/ties, metric queries, NVS/JSON roundtrips, v4 migration,
invalid imports, and 100 calls through real RTC restore helpers without creating
observations or writing progress. Existing migration suites cover versions 1–3;
layout tests render all new pages in both unit modes through the actual canvas.

Before your manual upload, export a v4 backup and keep flash erase disabled.
Check migration retains totals, dates, records, discoveries, gear, fur, setup
status and settings. Verify one new live fetch adds one observation and prints
all measurements; check both latest pages and four Records pages with A/C.
Switch US/Metric and compare records/dates without changing JSON values. Check
unset records show NOT OBSERVED until a valid measurement arrives. Repeated
cached sleep/wake must retain counts/records, raw temperature and Wi-Fi-off
behavior. Export/import v5, cancel before B, and verify legacy v4 backup import.
Confirm no trails/black flashes, and unchanged gear/blink/weather reactions,
buttons, sunrise/sunset/moon, timer, sound, deep sleep and B wake. Compare pressure
as surface pressure (altitude-dependent), not sea-level pressure. Wettest means
one observation interval, not a daily total. No hardware test or upload is automatic.


## Official severe-weather field events (firmware 0.6.0)

**FairWeather Friend is NOT an emergency warning device.** Severe-event collection
requires Wi-Fi and the user opening the buddy for an existing live weather check.
It does not poll in the background or wake independently. Events can be missed;
API/network failures can occur. Rely on official emergency-alert systems for safety.
The “NEW FIELD EVENT! / RECORDED” card is a collectible journal acknowledgment,
not an emergency notification or a substitute for official alerts.

Only an accepted live Open-Meteo observation triggers the supplemental NWS check.
The existing 60-second observation guard, provider, cache refresh interval and Wi-Fi
shutdown remain unchanged. A cached wake never requests NWS data or collects events.
NWS failure leaves the successful normal observation and cache intact. No alerts
are inferred from weather codes, wind, rainfall or descriptions, and missed periods
are never backfilled.

Requests use the currently effective saved/config latitude and longitude, formatted
to six decimals. A coarse U.S./territories prefilter skips clearly distant locations;
it does **not** guess country boundaries. In the same live session,
`https://api.weather.gov/points/<latitude>,<longitude>` must return a supported NWS
forecast-zone URL before requesting
`https://api.weather.gov/alerts/active?point=<latitude>,<longitude>`.
For example Toronto passes the broad prefilter but fails NWS coverage validation;
London skips NWS entirely. Unsupported locations or metadata failures skip the
alert request cleanly. Both requests send centralized headers:

```text
User-Agent: FairWeatherFriend/0.6.0 (https://github.com/jetgirlart/FairWeatherFriend)
Accept: application/geo+json
```

The version is generated from `FW_VERSION`, not duplicated in the client.
See the [official NWS API documentation](https://www.weather.gov/documentation/services-web-api).
Requests have 3-second connection/4-second stream timeouts, follow the API's
redirects, and parse directly from HTTP/1.0 streams with ArduinoJson filters.
Each response is bounded to 96 KiB, including unknown-length responses; alert
snapshots over 64 entries are skipped. Only IDs and exact `properties.event`
names are retained while parsing, and response documents are freed before saving.
The client uses the existing prototype's unverified TLS policy; certificate
verification remains a separate networking improvement.

Stable collectible IDs / exact official event mappings:

| ID | Internal ID | Official event name |
|---:|---|---|
| 0 | TORNADO_WATCH | Tornado Watch |
| 1 | TORNADO_WARNING | Tornado Warning |
| 2 | SEVERE_THUNDERSTORM_WATCH | Severe Thunderstorm Watch |
| 3 | SEVERE_THUNDERSTORM_WARNING | Severe Thunderstorm Warning |
| 4 | FLASH_FLOOD_WARNING | Flash Flood Warning |
| 5 | FLOOD_WARNING | Flood Warning |
| 6 | HURRICANE_WATCH | Hurricane Watch |
| 7 | HURRICANE_WARNING | Hurricane Warning |
| 8 | TROPICAL_STORM_WATCH | Tropical Storm Watch |
| 9 | TROPICAL_STORM_WARNING | Tropical Storm Warning |
| 10 | WINTER_STORM_WARNING | Winter Storm Warning |
| 11 | BLIZZARD_WARNING | Blizzard Warning |
| 12 | ICE_STORM_WARNING | Ice Storm Warning |
| 13 | EXTREME_HEAT_WARNING | Extreme Heat Warning |
| 14 | EXTREME_COLD_WARNING | Extreme Cold Warning |

Unknown names are ignored; a watch never implies a warning or vice versa. Each
category stores a uint32 lifetime count and UTC first/latest **encounter** epochs.
Discovered is derived from count > 0. Seeing the same ID again does not change its
category's count or first/latest encounter timestamps. A different alert ID of the
same type increments the count and advances latestAt. Alert issue/expiry dates are
not substituted for the time the buddy actually witnessed the event.

Deduplication uses 64-bit FNV-1a fingerprints of `properties.id`, falling back to
the feature's top-level `id`. Missing/empty IDs are skipped. The 32-entry cache
stores hash + last-seen epoch in the same NVS checkpoint as event records. It
survives power loss, migration checkpoints and current-version JSON roundtrips.
Every cached alert present in a response is pinned before insertion; replacement
chooses the least recently seen unpinned entry. With all 32 slots pinned, extra
IDs are skipped rather than evicting currently active alerts. This is bounded
recent deduplication, not an unlimited archive: a long-absent ID that has been
evicted can eventually count again; hash collisions are also theoretically possible.
A changed batch checkpoints once, and unchanged empty batches do not write flash.
The normal observation keeps its existing checkpoint; NWS results use a separate
meaningful batch checkpoint with the same retry and before-sleep policy.

`SAVE_VERSION` is now 6 (1104-byte payload, 1128 bytes with envelope). Versions
1–5 migrate without resetting any existing journal, metrics, records, discoveries,
gears, fur, settings, location or dates. Added event records and hashes start empty.
Existing completed buddies skip setup. Unknown/newer NVS saves remain protected.
Export JSON adds `fieldEvents`: all 15 records with numeric id, official name,
discovered, count, firstAt and latestAt; `recentAlerts` has 32 `[hexHash, seenAt]`
entries, preserving slot order for checksum validation. No alert bodies are exported.
Imports validate IDs/names, unique IDs/hashes, types, ranges, empty/discovered
consistency, timestamps and checksum. Old supported backups migrate with empty
events. JSON is limited to 8192 bytes, and the desktop tool permits at most 8205
command bytes including `IMPORT_BUDDY `. On-device B confirmation remains required.

Journal now has ten pages: the original five, followed by five SEVERE WEATHER
pages with three events each. A cycles pages; C returns to menu. Unseen events
show `--`. Each new category queues a 2.5-second HOME discovery card using the
existing frame scheduler and one display update. Menu/setup/focus screens take
priority. Cards do not keep the device awake or change inputs, sound or power.
`discoveredFieldEvent(id)` and the compact records provide future reward hooks;
no new gear or unlock rules are implemented.

Modules: `field_events.h/.cpp` own mapping/collection/deduplication;
`nws_client.cpp` owns supplemental HTTP requests; `save.h/.cpp`, `journal.h/.cpp`
own permanent data and checkpoints; `journal_ui.h/.cpp` and the HOME entry in
`display.cpp` compose pages/cards. No main sketch or hardware driver changes.

Run the new host suites from the repository root (omit `-isystem` on Linux):

```sh
mkdir -p /tmp/fwf-test-config
cp altoids-pet/tests/field_events/stubs/config.example.h /tmp/fwf-test-config/config.h
for suite in field_events network; do
  c++ -std=c++17 \
    -isystem "$(xcrun --show-sdk-path)/usr/include/c++/v1" \
    -I/tmp/fwf-test-config -Ialtoids-pet/tests/field_events/stubs -Ialtoids-pet/tests/weather/stubs \
    -Ialtoids-pet/tests/journal/stubs -Ialtoids-pet/firmware/altoids_pet \
    -I"$HOME/Documents/Arduino/libraries/ArduinoJson/src" \
    altoids-pet/tests/field_events/test_${suite}.cpp -o /tmp/fwf-${suite}-tests
  /tmp/fwf-${suite}-tests
done
```

Hardware checks after your own upload, with flash erase disabled:

1. Export and keep a v5 backup first. Verify v6 retains all prior progress,
   metric records, gear slots, fur, settings, location and research date; new
   event pages initially show `--`, and first-run setup does not repeat.
2. Open for an eligible live U.S. check. Serial should show normal measurements,
   then `NWS alerts: N active` or a concise supplemental failure. No-connection
   and zero-alert sessions should leave prior progress intact.
3. Sleep and B-wake within the existing weather cache interval. Confirm Wi-Fi
   stays off, no NWS diagnostics appear, and event counts remain unchanged.
4. When a supported official alert naturally affects the configured point,
   verify its new card and category count. After another actual live check and
   after unplugging/reconnecting, the same ID should not increment it again.
   A genuinely new alert increments only its exact category. Do not travel into
   severe weather to test; controlled host fixtures cover these cases safely.
5. Browse all ten Journal pages with one A press/release per page. Check long
   thunderstorm labels, counts, C navigation, notification expiry and no flashes.
6. Export/import a v6 backup with explicit B confirmation, then export again
   and compare events/hashes. Try cancellation and malformed data; the current
   buddy must remain intact. Restoring an old backup intentionally restores that
   old buddy and leaves severe events empty, so keep the newer backup first.
7. If testing alternate locations, temporarily use London (NWS skipped) and
   Toronto (coverage lookup fails without requesting alerts), then restore your
   normal coordinates. Confirm ordinary weather still succeeds.
8. Recheck minute updates, pet/gear/weather animations, timer completion, sound,
   30-second inactivity sleep and B wake. Focus mode must not be interrupted by
   a queued discovery card.
