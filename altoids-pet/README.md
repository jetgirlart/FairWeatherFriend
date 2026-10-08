# FairWeather Friend

FairWeather Friend is a pocket-sized virtual field-research buddy that logs
weather observations when you open it. The friendly kitsune is a self-sufficient
little meteorologist: it needs no feeding, cleaning, care, or friendship grinding.
There is no hunger, health, neglect, punishment, or death. Progress comes from
observing weather, setting lifetime records, discovering weather types, and
unlocking cosmetic field gear. Missed weather is intentionally missed.

## Hardware and wiring

The working hardware is a **Seeed Studio XIAO ESP32-S3**, an **Adafruit SH1107
128×128 monochrome I2C OLED**, and three momentary buttons. Keep the existing
power wiring and OLED supply/address configuration.

| Connection | XIAO pin | GPIO | Wiring |
| --- | --- | --- | --- |
| Button A | D0 | 1 | Button to GND |
| Button B / wake | D1 | 2 | Button to GND |
| Button C | D2 | 3 | Button to GND |
| Optional passive piezo | D3 | 4 | D3 → 220 Ω → piezo +; piezo − → GND |
| OLED SDA | D4 | 5 | OLED SDA |
| OLED SCL | D5 | 6 | OLED SCL |
| Common ground | GND | — | OLED/buttons/piezo GND |

Buttons are active LOW with pull-ups. OLED address remains `0x3D`; I2C remains
400 kHz. See [Seeed's board guide](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/)
for pinout and programming recovery. Battery voltage monitoring is not implemented.
The XIAO hardware handles charging; firmware does not change it.

## Arduino setup, configuration, and upload

Use the working board options and installed library versions. Select
**XIAO_ESP32S3** in **esp32 by Espressif Systems**. The current full build was
verified with Arduino-ESP32 **3.3.12**.

Existing libraries:

- Adafruit GFX Library and Adafruit SH110X, with their existing dependencies.
- ArduinoJson (current installed version 7.4.3, `JsonDocument` API).
- Core-provided Wire, WiFi, WiFiClientSecure, HTTPClient, Preferences, LEDC,
  time/NTP, and RTC/deep-sleep APIs. No new Arduino libraries are required.

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
│   ├── weather.cpp / weather.h
│   ├── power.cpp / power.h
│   ├── timer.cpp / timer.h
│   ├── sound.cpp / sound.h
│   ├── journal.cpp / journal.h
│   ├── journal_ui.cpp / journal_ui.h
│   ├── gear.cpp / gear.h
│   ├── save.cpp / save.h
│   ├── config.example.h
│   └── config.h                  # local, gitignored
├── tests/journal/                # save/observation host checks
├── tests/journal_ui/             # screen/control host checks
├── hardware/
├── enclosure/
├── assets/sprites/
├── docs/
├── README.md
├── LICENSE
└── .gitignore
```

- `display`: existing OLED framebuffer/partial-update driver, UI, and buttons.
- `pet`: temporary visual moods, B reaction, sleep schedule, and non-blocking
  idle/weather animation state machines. No permanent care/progression stats.
- `sprites`: PROGMEM kitsune artwork and bitmap composition.
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

`BuddySaveData` in `save.h` is **saveVersion 1**. It contains the research start
(`createdAt`, exported as `researchBeganAt`), observation/day totals, latest
observation, dated high/low records, eight category counters, discovery flags,
unlocked gear flags, and equipped gear ID. A zero start timestamp means the
existing clock has not become valid yet. It is captured once and never reset
by observations, sleep, reboot, or firmware upload. A confirmed Import Buddy is
an explicit replacement with the imported buddy's own start date.

Permanent progress lives in **NVS namespace `fwf-buddy`**, keys `save0`/`save1`.
Each record has a stable header (magic, version, length, generation, checksum)
and an explicit 148-byte little-endian payload. Native structs, padding, mood,
Wi-Fi settings, and RTC caches are not serialized. Writes alternate slots and
are read back/verified; load selects the newest valid slot and can fall back
from a checksum-damaged slot.

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
starts at NONE. `equipGear()` rejects locked/invalid items and checkpoints a
changed selection. The GEAR screen lets you browse/select equipment. **Drawing
the selected equipment on the home pet remains future artwork/rendering work.**
Existing weather umbrella/scarf reactions continue independently.

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
- **GEAR:** Opens at the equipped item. A cycles NONE and the seven gear IDs,
  including locked items. Each displays EQUIPPED, UNLOCKED, or LOCKED, plus its
  requirement. B equips an unlocked choice; NONE removes equipment. Locked
  choices cannot be equipped. SAVE FAILED leaves the previous equipment intact.
  Selecting an already-equipped item does not write NVS again.

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

The flicker-free driver still composes one framebuffer and sends changed OLED
bytes/pages. Battery status is not added. The home time,
weather icon/temperature, sunrise/sunset, moon phase, and animations keep their
existing layout and scheduler. RTC wake setup still guards against an asserted
B input before blanking the screen and entering deep sleep; only the permanent
checkpoint hook changed.

B on awake HOME keeps its 700 ms hop/heart, temporary EXCITED then HAPPY mood,
and optional sound. It has no lifetime counter or friendship reward. Night-time
sleep remains authoritative. Occasional blink/double blink, looks, bounce, ear
twitch, and weather reactions remain non-blocking at the existing frame cadence.

The original PROGMEM kitsune assets remain in `sprites.cpp`: nine base sprites
plus LOOK_UP. Each is **24×24, 72 bytes**, three MSB-first bytes per row, white
bits on a transparent background. They render at crisp 2× scale. To replace an
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
checks every text/circle stays inside 128×128. They cover all Journal pages,
record/empty displays, all gear choices and locks, equip/unequip, duplicate-write
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

Source comparisons check that networking/time/cache, power logic, the OLED
partial-update driver, idle/weather scheduler, sprites, timer, sound, save model,
and pin configuration remain unchanged by the new screens. The full
XIAO ESP32-S3 build is also verified. No hardware upload/test was performed here.

On the physical device:

- Open JOURNAL, cycle all four pages, and compare totals, dates, latest weather,
  counts and discovery dots with exported JSON. Confirm B changes nothing and
  C returns to the same menu entry.
- Open RECORDS and check positive/negative temperatures and dates against JSON.
  Check empty-journal messages using the host tests or a deliberately confirmed
  test import after exporting your real buddy.
- Browse all eight GEAR choices. Verify locked-item rejection, equip an unlocked
  item, select NONE to remove it, and reopen the screen to check EQUIPPED status.
  Reboot/reflash with normal NVS preservation and verify the selection persists.
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

## License

A license has not yet been selected; see `LICENSE`.
