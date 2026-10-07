# Altoids Pet

Working Arduino pet firmware for a Seeed Studio XIAO ESP32-S3 and an Adafruit
SH1107 128 × 128 monochrome OLED. This first refactor separates responsibilities
while preserving the prototype's UI, timings, networking, caching, and power
behavior. The original `../testsketch` remains the reference.

## Hardware and wiring

Use the existing XIAO ESP32-S3, OLED, three momentary buttons, and a USB data
cable for programming. Keep the prototype's power wiring.

| Connection | XIAO pin | ESP32-S3 GPIO | Wiring |
| --- | --- | --- | --- |
| Button A | D0 | GPIO1 | Button between D0 and GND |
| Button B / wake | D1 | GPIO2 | Button between D1 and GND |
| Button C | D2 | GPIO3 | Button between D2 and GND |
| Optional piezo | D3 | GPIO4 | D3 → 220 Ω → piezo +; piezo − to GND |
| OLED SDA | D4 | GPIO5 | OLED SDA to D4 |
| OLED SCL | D5 | GPIO6 | OLED SCL to D5 |
| OLED ground | GND | — | OLED GND to common ground |

All buttons use `INPUT_PULLUP` and are pressed when LOW. Before deep sleep,
B is explicitly configured as an RTC input with its pull-up enabled and
pull-down disabled. On boot it is returned to digital GPIO mode before button
initialization. EXT0 remains B-only and LOW-triggered. If B reads LOW before
sleep (including after RTC setup), the OLED stays on and sleep is deferred for
another 30 seconds; a setup error also defers sleep and is logged. This prevents
entering sleep with the wake condition already asserted. See
[ESP32-S3 EXT0 sleep guidance](https://docs.espressif.com/projects/esp-idf/en/release-v5.4/esp32s3/api-reference/system/sleep_modes.html).

The OLED remains at
I2C address `0x3D`, with a 400 kHz bus. Keep its existing supply connection and
any existing address configuration. No additional hardware is required.

Pin mappings and board setup are documented in
[Seeed's XIAO ESP32-S3 guide](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/).

## Arduino requirements

Keep the same board core version, library versions, and board options used by
the working sketch for this first refactor. In Arduino IDE, the board is
`XIAO_ESP32S3` in the **esp32 by Espressif Systems** board package.

Required libraries, already used by the prototype:

- **Adafruit GFX Library** (`Adafruit_GFX.h`).
- **Adafruit SH110X** (`Adafruit_SH110X.h`).
- **ArduinoJson**, using the existing `JsonDocument` API.

Retain the dependencies already installed with those libraries. `Wire`, `WiFi`,
`WiFiClientSecure`, `HTTPClient`, time/NTP, and deep-sleep/RTC support come from
the ESP32 Arduino environment. Pet checkpoints use `Preferences`, also included
with the ESP32 core; no separately installed libraries or frameworks were added.

## Configuration

From `altoids-pet/`, create the local settings file if it does not already exist:

```sh
cp firmware/altoids_pet/config.example.h firmware/altoids_pet/config.h
```

Edit **only `firmware/altoids_pet/config.h`** with your Wi-Fi name/password and approximate
latitude/longitude. The example contains placeholder credentials and sample
coordinates. Both Git ignore files exclude `config.h`, including copies in
other sketch folders. Keep credentials out of `config.example.h` and the
reference sketch.

The local configuration retains the prototype defaults:

| Setting | Default |
| --- | --- |
| Central timezone with automatic DST | `CST6CDT,M3.2.0/2,M11.1.0/2` |
| Pet awake hours | 07:00 to before 22:00 |
| Inactivity deep sleep | 30,000 ms |
| Weather cache refresh threshold | 3 hours |
| Home animation interval | 250 ms |

## Build and upload

1. Use the working sketch's Arduino IDE and installed ESP32 core. If setting up
   another computer, install that same core and the libraries above. See the
   linked Seeed guide for board-package installation.
2. Open `firmware/altoids_pet/altoids_pet.ino` directly in Arduino IDE. The
   sketch folder already matches the `.ino` filename, and all `.cpp` and `.h`
   files are alongside it so Arduino compiles the modules together.
3. Select **XIAO_ESP32S3**, preserve the prototype's other board options, and
   select the board's USB port.
4. Click **Verify**, then **Upload**. Open Serial Monitor at **115200 baud** to
   see boot, time sync, weather, and sleep messages.
5. If the USB port is unavailable or upload fails, use the board's BOOT/reset
   procedure in the Seeed guide, select the newly appearing port, and retry.

## Project structure

```text
altoids-pet/
├── firmware/
│   └── altoids_pet/
│       ├── altoids_pet.ino
│       ├── display.cpp / display.h
│       ├── pet.cpp / pet.h
│       ├── sprites.cpp / sprites.h
│       ├── weather.cpp / weather.h
│       ├── power.cpp / power.h
│       ├── timer.cpp / timer.h
│       ├── sound.cpp / sound.h
│       ├── config.example.h
│       └── config.h             # local, gitignored
├── hardware/
├── enclosure/
├── assets/sprites/
├── docs/
├── README.md
├── LICENSE
└── .gitignore
```

- `altoids_pet.ino` coordinates initialization and each loop iteration in the
  original order.
- `display` owns the OLED, rendering, weather graphics, screen/menu state, and
  the existing A/B/C menu actions.
- `pet` owns pet drawing, reactions, the sleep schedule, blinking, and animation
  state/updates, persistent mood/friendship, lifetime interactions, and birthday.
- `sprites` owns the original 24×24 kitsune bitmaps and their crisp 2× rendering.
- `weather` owns Wi-Fi/NTP sync, timezone, local clock and moon calculations,
  Open-Meteo parsing, sunrise/sunset, weather mapping, and the RTC weather cache.
- `power` owns button pins, polling/debounce, activity tracking, wake-cause
  detection, and deep sleep. Its inactivity gate skips sleep only while a timer
  is actively counting down.
- `timer` owns focus presets, monotonic countdown, RTC resume, completion,
  and timer-screen button actions.
- `sound` owns optional passive-piezo cues, non-blocking tone timing, rate
  limiting, sleep suppression, and output shutdown.

State is defined once in its owning module. Headers expose shared state and
functions needed by the other modules. The cache fields and online sync
timestamp retain `RTC_DATA_ATTR`. The pet uses bitmap artwork; existing UI and
accessory graphics retain their drawing commands. The other directories are
reserved for project materials.

## Preserved behavior

Cold boot attempts Wi-Fi/NTP and Open-Meteo sync. Central timezone/DST is
restored on every boot. On B-button wake, the firmware restores time and weather
from RTC memory; it attempts an online refresh when the cache is missing or
its valid sync timestamp is more than three hours old. The original timestamp
checks and failure fallbacks remain intact. This is a wake-time cache check,
not a new periodic online refresh.

- A opens the menu from home and cycles its entries.
- B reacts with the pet while awake, opens WEATHER or TIMER from the menu, or
  logs the selected PET or SETTINGS entry.
- C returns from weather to the menu, or otherwise returns home.
- Pet sleep remains from 22:00 until 07:00. Reaction duration, blinking,
  weather animations, moon phases, sunrise/sunset, and screen drawing are
  preserved.
- Clock checks remain every five seconds while time is valid, with clock-driven
  home redraws when the displayed minute changes.
- Outside an active focus timer, after 30 seconds without an accepted button
  press, the OLED turns off and the
  ESP32 enters deep sleep. B wakes it through EXT0 at a LOW level.

## Persistent pet state

The pet is a friendly companion. Friendship never decreases due to time away,
and there are no neglect, hunger, sickness, or death mechanics. The state lives
in `pet.cpp`; `getPetState()` exposes a read-only view for future use. It is not
added to the OLED UI or menus in this implementation.

- **Mood:** CALM by default while awake. An accepted home-screen B interaction
  makes it EXCITED during the existing hop/heart reaction, then HAPPY until five
  seconds after the interaction, then CALM. The existing 22:00–07:00 schedule
  overrides this with SLEEPY. CURIOUS is available for future behavior.
- **Friendship:** starts at 0, capped at 100. The first successful interaction
  earns one point; subsequent points require ten minutes between awards.
  Rapid presses still count as interactions, but do not earn extra friendship.
- **Interactions:** a lifetime 64-bit count of accepted B interactions on HOME
  while the pet is awake. Menu actions, sleeping-pet presses, and wake itself
  do not increment it. Existing debounce and held-button behavior remain intact.
- **Birthday:** Unix creation time, set once using the existing valid clock.
  If the pet starts offline without valid time, it remains 0 (unknown) until
  that clock becomes valid. It then records the first available valid time;
  the firmware does not invent an earlier date or change time synchronization.

RTC memory retains every state change across deep sleep, including friendship
and checkpoint cooldowns. With valid time, elapsed sleep/offline time counts
against cooldowns; without valid time, only awake `millis()` counts. Inactivity
never awards points automatically and never removes them. Temporary interaction
moods settle to CALM or SLEEPY on boot, according to the sleep schedule.

Versioned checkpoints use the ESP32 core's
[Preferences/NVS API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/preferences.html)
in namespace `altoids-pet`. Two alternating keys, `state0` and `state1`, hold
sequence-numbered records with checksums. Boot validates record size, version,
checksum, and bounds, then selects the newest valid NVS checkpoint. On deep-sleep
wake, valid RTC state takes priority when it is at least as recent; a newer NVS
checkpoint takes priority over older RTC state. Ordinary reboot/power-up restores
from NVS. The previous firmware's single `state` record is imported automatically
without resetting progress or birthday; do not erase NVS during the upgrade.

A known birthday is also stored once under `birthday`. This immutable value
remains authoritative even if a damaged progress checkpoint forces fallback to
an older one that did not yet know the birthday. No valid existing birthday is
replaced by the current boot time.

NVS writes happen at these checkpoints:

- **First initialization or legacy migration:** save the initial/imported record.
- **First valid birthday:** save the immutable birthday and updated checkpoint.
  The birthday key is written once; later checkpoints do not rewrite it.
- **Friendship gain:** save changed progress on the next normal pet update.
  Friendship gains remain limited to one per ten minutes.
- **Before normal deep sleep:** save any dirty progress, even inside the periodic
  cooldown. This commits batched interactions before the device sleeps.
- **While continuously awake:** save interaction-only changes when ten minutes
  have elapsed since the last successful checkpoint.

Clean state, mood changes, animation frames, and unchanged wake/sleep cycles do
not write flash. Multiple button presses are combined into one later checkpoint;
there is no unconditional write on each B press. Writes are read back and
validated before marking progress clean. Failed writes retain dirty RTC state
and wait ten minutes before retrying, including sleep-hook attempts. If NVS
cannot be opened/read at boot, persistence is disabled for that session to avoid
overwriting an unread pet; recovery is retried on the next boot.

**Power-loss boundary:** friendship, interactions, and birthday survive power
loss once a checkpoint has succeeded. After normal deep sleep, all accepted
interactions are checkpointed if storage is working. Sudden power removal while
awake can still lose interactions after the last checkpoint: preserving every
press immediately would require writing each press. A failed checkpoint also
cannot guarantee power-loss recovery. RTC retains these changes across deep
sleep while powered.

Serial Monitor explicitly reports `Pet loaded from RTC`, `Pet loaded from NVS`,
or `Pet loaded from newly initialized`, followed by friendship, interactions,
and birthday. Save messages identify sleep versus other checkpoints, and failures
are reported. Mood is internal state, accessible through `getPetState()`.

## Idle personality and expressions

The pet remains mostly still, with disposable runtime expression state in
`pet.cpp`. Idle animation never changes mood, friendship, interaction count,
birthday, activity/sleep timers, or NVS checkpoints.

| Idle state | Appearance | Duration |
| --- | --- | --- |
| REST | Original pose | Between actions |
| BLINK | Closed eyes, then normal eyes | 250 ms closed |
| DOUBLE_BLINK | Closed → open → closed → normal | Three 250 ms phases |
| LOOK_LEFT / LOOK_RIGHT | Eyes shift 2 pixels | 1 second |
| BOUNCE | Whole pet moves up 1 → 2 → 1 pixels, then settles | Three 250 ms phases |
| EAR_TWITCH | Left ear tip shifts 1 → 2 pixels, then settles | Two 250 ms phases |

| Mood | Blink interval after a blink | Special-action wait | Preference |
| --- | --- | --- | --- |
| CALM | 4–7 seconds | 10–18 seconds | Quiet looks/twitches; 30% of opportunities remain REST |
| HAPPY | 3–5 seconds | 2–4 seconds | More bounce and double blink |
| CURIOUS | 3.5–5.5 seconds | 6–10 seconds | Mostly left/right looking |
| EXCITED | 2–3.5 seconds | 1.5–3 seconds | Mostly bounce/double blink |
| SLEEPY | 7–10 seconds | No special motion | 500 ms closed blink if awake |

Actual sleeping pets keep the original closed-eye pose and perform no awake
idle actions, including blinking. B's existing six-pixel hop/heart takes priority
and cancels idle motion. The existing mood lifecycle still makes EXCITED brief
and HAPPY last five seconds; CURIOUS is supported for future mood behavior but
is not newly assigned. Timing varies with a small local generator; it does not
use a library, affect other randomness, or persist.

The initial and C-return-home blink deadline remains three seconds. Action
waits are scheduled when awake idle resumes, when the mood changes, and after a
special action finishes. Blink timing is rescheduled after single/double blinks.
Actions run one at a time and advance with `millis()`, without delays or catch-up
bursts. Ordinary blinks may postpone a special action slightly.

Idle stepping now occurs only on an existing 250 ms home frame. This makes each
blink/twitch/bounce phase visible instead of completing between OLED pushes.
The weather `animationFrame` cadence is unchanged. The existing home framebuffer
is composed once in RAM, with no intervening blank OLED frame. The display layer
compares the finished image with the last transmitted image and sends only the
changed column span on each changed eight-pixel-high OLED page. An unchanged
image sends no pixel data; clock changes do not rewrite unchanged pet/weather
regions. Menus/weather pages do not run home idle animation.

Partial transfers use the existing Adafruit I2C interface and SH1107 page
protocol, address, and clock speeds. They require an extra 2 KB RAM snapshot,
not another library. The first transfer initializes the whole image. A failed
I2C write invalidates the snapshot so the next transfer resynchronizes the image.
Menu transitions naturally transfer every changed region without stale pixels.

On hardware, watch HOME for about 20–25 seconds to catch a normal blink and an
occasional look, double blink, bounce, or twitch before the unchanged 30-second
sleep. Repeated quiet sessions may be needed to see all weighted actions. Press B
during an idle action to verify the original hop/heart wins; afterward watch for
a livelier HAPPY expression. Check C/home and menu navigation, the sleeping pose
at 22:00–07:00, normal B wake, unchanged weather effects, and absence of black
flashes. Verify idle-only sessions leave interaction counts and NVS save activity
unchanged. CURIOUS/EXCITED/SLEEPY-awake profiles are covered by host tests; seeing
all of them on hardware would require a temporary diagnostic build, since the
current mood lifecycle does not hold those awake moods for long.

## Occasional weather reactions

Weather reactions share the existing idle state machine and 250 ms frame
scheduler. They use the current weather state and existing `isDaylight()` result,
without fetching weather, changing time, assigning moods, or saving pet progress.

| Existing weather | Reaction | Duration |
| --- | --- | --- |
| CLEAR / MAINLY CLEAR, daytime | Small 1 → 2 → 1 pixel happy bounce | 750 ms |
| CLEAR / MAINLY CLEAR, night | Eyes look upward/right toward sky | 1 second |
| RAIN | Chunky umbrella beside pet, tiny canopy bob | 1.5 seconds |
| SNOW | Scarf and four alternating 1-pixel shiver steps | 1 second |
| STORM | Closed-eye crouch, head 3 pixels lower, feet stay grounded | 1 second |
| FOG | Look left, then right | 1.5 seconds |
| CLOUDY / PARTLY CLOUDY / UNKNOWN | Ordinary mood/idle behavior | No extra reaction |

The first weather opportunity occurs 12–22 seconds after awake HOME idle resumes.
After completion, another opportunity waits 25–45 seconds. Blinks take priority,
and an active idle action finishes before a weather action starts. Weather actions
also finish before another idle action starts. Existing mood-based idle choices
remain available between reactions. With a 30-second inactivity timeout, some
sessions may sleep before a weather opportunity can run; reactions are intentionally
occasional.

B's hop/heart, sleep, menu navigation, and C's existing reset suppress/cancel
weather reactions. A reaction is suppressed immediately and canceled on the next
frame if weather validity, weather state, or daylight changes. Props are drawn
inside `drawPet()` below the header and above the ground/temperature text. The
same complete RAM framebuffer and partial OLED transfer are used: no extra display
pushes, blank frames, delays, libraries, menus, or settings.

Hardware checks:

1. During awake HOME sessions, watch 20–25 seconds for the reaction matching
   current weather. Repeat sessions if normal blinks/idle actions postpone it.
2. Check sunny bounce, clear-night upward eyes before the 22:00 sleep hour,
   rain umbrella, snow scarf/shiver, storm crouch, and fog left/right glance.
   Cloudy/partly cloudy should keep the ordinary calm idle behavior. To exercise
   unavailable weather locally, a temporary diagnostic build can override only
   `weatherState`/`weatherValid` in RAM after normal weather initialization;
   no production override or settings menu was added.
3. Press B during a weather action: verify the same hop/heart, one interaction,
   and no remaining prop. Check C/menu return, sleeping closed-eye pose, normal
   30-second sleep, and B wake. No awake reaction should run while sleeping.
4. Verify the umbrella/scarf/crouch leave no trails or overlap clock, weather
   icon, temperature, or label. Watch weather effects, idle blinking, and minute
   changes for flicker. Reaction-only sessions should add no interactions,
   friendship gains, or NVS saves.

## Timer / Focus Mode

Select the existing TIMER menu item with B. Setup offers **5, 10, 15, and 25
minutes**, starting at 5 minutes on a new timer record.

| Screen | A | B | C |
| --- | --- | --- | --- |
| Timer setup | Cycle presets | Start selected timer | Return to menu |
| Running focus | No action | No action | Cancel and return to menu |
| DONE | No action | No action | Return home |

The running screen shows large `MM:SS` text and a quiet pet with a small open
book. It redraws only when the remaining displayed second changes. Home idle
and weather animations pause on timer screens; their scheduler and partial OLED
transfer remain unchanged. A pet inside its normal sleep hours keeps the sleeping
pose instead of reading or celebrating.

Countdown uses unsigned `millis()` deltas and continues across rollover, without
animation delays. The 30-second inactivity sleep gate is suppressed only while
the countdown is running. No new wake source or alarm is added. When it finishes,
DONE appears with a short 1.5-second bounce/heart celebration for an awake pet.
Completion grants a fresh 30-second DONE viewing window, then normal inactivity
sleep applies. B in focus/DONE does not interact with the pet or restart the timer.
The timer adds no vibration, progress rewards, or NVS writes. Its optional
completion cue is handled by the separate sound module.

`timer.cpp` keeps a separate magic/version/checksum-validated RTC record marked
`RTC_NOINIT_ATTR`; this avoids startup reinitialization on supported warm resets
and retains state across deep sleep. It stores the selected preset, phase,
remaining milliseconds, and a UTC deadline when the existing clock is valid.
See Espressif's
[RTC memory attributes](https://github.com/espressif/esp-idf/blob/master/components/esp_common/include/esp_attr.h).
This record does not change pet RTC/NVS storage.

After the normal boot weather/time and pet initialization, a valid active timer
resumes on the focus screen. If valid system time and a saved deadline are
available, elapsed reset/sleep time is included; an elapsed deadline opens DONE.
Backward wall-clock changes cannot increase the saved remaining duration.
Normal live countdown always uses `millis()`, so a wall-clock correction cannot
jump it. If a reset occurs without a valid UTC clock/deadline, it resumes the
saved remaining duration; time spent powered down/resetting cannot be recovered
in that fallback. UTC recovery has approximately one-second resolution.

RTC retention depends on reset cause and power remaining available. Complete
power removal loses the timer; it is deliberately not written to flash. Invalid
or interrupted RTC records are rejected safely. Setup/cancel/DONE dismissal clear
running status. Existing boot Wi-Fi/weather decisions remain unchanged when a
timer is resumed; no extra synchronization is requested.

Physical-device checklist:

1. Open MENU → TIMER. Verify A cycles 5 → 10 → 15 → 25 → 5, B starts the chosen
   value, and C from setup returns to the same menu selection.
2. Start 5 minutes and leave the device untouched for longer than 30 seconds.
   Verify it stays awake, decrements once per second, and shows the pet/book
   clearly without flicker or leftover weather/idle graphics.
3. Verify A/B during focus do not restart the countdown or add pet interactions.
   C cancels to the menu, after which normal 30-second sleep/B wake still works.
4. Let a 5-minute timer finish. Verify `00:01` transitions to DONE, the short
   awake-pet celebration, no immediate sleep, and C returns home. Leaving DONE
   untouched should resume normal sleep after 30 seconds.
5. Select/start/cancel the longer presets to check their initial values. Complete
   a 25-minute session when convenient to confirm long-session sleep inhibition.
6. During an active timer with valid time, use a warm reset while keeping power
   connected. Check the resume log and remaining time, including normal boot
   latency. If its deadline elapsed during reset, it should show DONE.
7. Verify normal sleeping-pet appearance if a focus session crosses 22:00;
   check weather/time, menu navigation, pet counts/birthday, cache use, and B wake
   afterward. Timer use alone must not change pet progress.

## Optional passive piezo sound

Use a small **passive piezo buzzer/element**, driven from the unused XIAO **D3
(GPIO4)**. This is a two-terminal piezo connection, not a powered speaker module.

```text
XIAO D3 (GPIO4) ── 220 Ω resistor ── piezo +
XIAO GND ────────────────────────── piezo −
```

If the element has no polarity marks, connect one terminal through the resistor
and the other to GND. It needs no separate 3V3/5V supply. Buttons stay on D0/D1/D2
and OLED I2C stays on D4/D5. The resistor limits current spikes; volume depends on
the piezo's resonance and mounting. Sound remains optional: leaving the piezo
unconnected does not affect firmware behavior.

`config.example.h` provides:

```cpp
#define PIEZO_PIN D3
#define SOUND_ENABLED true
#define SOUND_STARTUP_CHIRP false
```

For an existing private `config.h`, append these settings there to customize
sound without replacing credentials or other settings. Older configs also work:
`sound.cpp` supplies these defaults if they are absent. Set `SOUND_ENABLED false`
for global silence; set `SOUND_STARTUP_CHIRP true` for the optional startup/wake
chirp. There is no settings menu or persisted sound preference.

| Event | Cue | Policy |
| --- | --- | --- |
| Successful awake HOME B interaction | 2400 Hz, 55 ms | At most one chirp per second; all accepted interactions still count |
| Timer finishes | 2000 Hz, 90 ms; 60 ms silent gap; 2600 Hz, 120 ms | Once at completion, including during pet sleep hours |
| Startup / B wake | 1600 Hz, 35 ms | Optional, default OFF; silent while pet sleeps |

Timer completion takes priority over other cues, which cannot interrupt it.
Other cues stop if the pet enters sleep hours. Cues do not queue into melodies,
and failed PWM setup disables sound for that boot. Timer/interaction chirps
are scheduled at their existing events without altering pet progress or timer
state. Resuming an expired RUNNING timer emits completion once; restoring an
already DONE timer does not replay it.

The module uses the installed Arduino-ESP32 3.x LEDC API (verified with core
3.3.12); no new library is needed. Hardware generates the tone while the main
loop remains free. `updateSound()` advances short steps using rollover-safe
`millis()` deltas, with no animation delays. PWM detaches and the piezo output
returns LOW between cues and immediately before actual deep sleep. Sleep is
never delayed to finish a sound. Display transfers, Wi-Fi, clock, weather,
RTC/NVS progress, button controls, and the RTC B wake setup are unchanged.

Physical sound checklist:

1. Wire the passive piezo as above, upload with the existing board options, and
   confirm one short chirp on an awake HOME B interaction with the same hop/heart.
2. Press B rapidly: sounds should be rate-limited, while accepted interaction
   counts and controls retain their existing behavior. Menu/setup button presses
   should be silent.
3. Run a 5-minute timer to DONE: hear two brief tones separated by a short gap.
   Test completion during sleep hours too; this is the only night-time cue.
4. During pet sleep hours, B/startup/wake should be silent. With startup chirp
   enabled temporarily, verify awake cold boot and B wake chirp once.
5. Set `SOUND_ENABLED false`, rebuild, and verify all cues are silent. Restore
   `true` afterward if desired.
6. Check the normal 30-second sleep leaves the buzzer silent, B wake stays stable,
   and countdown, idle/weather poses, clock updates, and partial OLED updates
   remain responsive and flicker-free. Verify pet counts/birthday and weather
   caching behave as before.

## Kitsune sprite artwork

The permanent pet is an original outline-oriented monochrome kitsune with large
pointed ears, a compact body, little feet, and a curled tail. `sprites.cpp` holds
the PROGMEM assets; `sprites.h` declares the names and dimensions. Every sprite
contains 72 bytes: 24 rows of three bytes, with the leftmost pixel in each byte's
most significant bit. `1` is white, `0` is transparent over the framebuffer.
The row comments show the artwork using `#` and `.`.

The source art is **24×24**, presented at an integer **2× scale (48×48)** on the
OLED. A temporary 288-byte RAM bitmap is expanded without smoothing and passed
to Adafruit_GFX's `drawBitmap()` RAM overload. It composes into the existing
framebuffer without clearing it or transferring to the OLED. The sprite is
positioned 16 pixels down inside the previous pet anchor to retain the feet
baseline and existing home, interaction, focus, and DONE placements.

| Mood / action | Bitmap |
| --- | --- |
| CALM, CURIOUS at rest; quiet focus pose | `KITSUNE_IDLE` |
| HAPPY / EXCITED / SLEEPY while awake | `KITSUNE_HAPPY` / `KITSUNE_EXCITED` / `KITSUNE_SLEEPY` |
| B interaction | `KITSUNE_EXCITED`, with the existing hop and heart |
| Blink / double-blink closed phases | `KITSUNE_BLINK` |
| Left / right idle look and fog look | `KITSUNE_LOOK_LEFT` / `KITSUNE_LOOK_RIGHT` |
| Idle or sunny bounce | `KITSUNE_BOUNCE`, with the existing 1–2 pixel offset |
| Clear-night upward look | `KITSUNE_LOOK_UP` (additional weather expression) |
| Ear twitch | Current bitmap, with its upper-left ear tip shifted 1 then 2 OLED pixels |
| Storm crouch | `KITSUNE_BLINK`, torso shortened three OLED rows with the existing +3 y offset |
| DONE while awake | `KITSUNE_HAPPY`, with the existing brief bounce/heart |
| Authoritative sleep hours | `KITSUNE_SLEEP`, overriding all awake expressions |

Rain and snow keep the umbrella and scarf as separate overlays. Snow retains
the existing one-pixel alternating shiver; only scarf placement is adjusted to
the new neck. Focus keeps its separate book, and hearts/weather backgrounds are
unchanged. Sleep overrides blinking, moods, and actions. CURIOUS continues to
use occasional looks rather than looking continuously. The existing idle and
weather action machine, timing, display transfer strategy, and pet storage are
unchanged; rendering never writes pet progress.

To replace one expression later:

1. Draw a 24×24 one-bit image, retaining the current ear-tip region, neck and feet
   alignment if you want existing overlays/transforms to line up.
2. Export horizontal, MSB-first bytes in top-to-bottom row order, with white
   pixels represented by `1`: exactly three bytes per row and 72 bytes total.
   Do not use XBM's LSB-first format.
3. Replace only that named array's initializer in `sprites.cpp`, retaining its
   symbol, dimensions, and `PROGMEM`. Update the row comments for readability;
   comments alone do not change the bitmap. No pet-state logic changes are needed.
4. Verify/upload with the existing board settings. Check the expression and its
   overlay alignment on the actual OLED.

Physical OLED checklist:

1. Check the ears, simple face, feet and curled tail on HOME; confirm clock,
   temperature, weather label and ground remain unobstructed.
2. Observe normal and double blinks, left/right looks, 1–2 pixel bounces and ear
   twitches. Press C to check the existing blink restart. Confirm no trails or
   black flashes, including when the displayed minute changes.
3. Press B during an idle/weather pose: confirm the same hop/heart and sound,
   then the normal mood return. Check that accepted interactions still count.
4. With applicable cached weather, check sunny bounce, night upward gaze, rain
   umbrella, snow scarf/shiver, storm crouch and fog looks. Confirm accessories
   disappear when their reaction ends and neutral weather stays calm.
5. At sleep hours, confirm the closed-eye sleep sprite and absence of awake
   idle/weather actions. Check sleeping appearance during focus and DONE too.
6. Run TIMER: verify the book covers the torso without hiding the face/countdown,
   the countdown keeps running, and DONE retains its happy bounce/heart and cue.
7. Confirm normal 30-second inactivity sleep and B wake, cached-weather/Wi-Fi-off
   wake, and the existing RTC/NVS friendship/count/birthday restore logs.

## Validation

Kitsune host checks verify all ten assets' pixel expansion, mood/action selection,
blink/double-blink phases, ear/bounce/shiver offsets, weather timing and overlays,
storm baseline, sleep/B/focus priority, frame gating, and no animation NVS writes.
Source comparisons confirm the pet persistence and idle/weather scheduler and
the entire display implementation are unchanged from before the sprite change.
The full XIAO_ESP32S3 firmware build passes with the installed ESP32 core 3.3.12.
No upload or physical OLED test was performed for this change.

Sound host tests pass for cue/gap durations, rate limiting, timer priority,
sleep-hour suppression/exception, immediate shutdown, optional startup,
global disable, rollover, and PWM setup failure. The full firmware compiles
with the installed XIAO_ESP32S3 board definition and Arduino-ESP32 core 3.3.12.
No upload or physical piezo test was performed.

The original refactor was verified against `testsketch`. Timer host tests cover
menu/preset controls, active sleep inhibition, per-second redraw gating, DONE
viewing window, cancellation, RTC/deadline recovery, unavailable-clock fallback,
invalid records, backward clock corrections, millis rollover, sleeping pets,
and absence of timer pet/NVS writes. Full stub compile/link checks pass. Weather-reaction host
tests cover all poses/durations, neutral weather, sleep/menu/B/C priority,
weather/daylight cancellation, idle/blink coexistence, no reaction NVS writes,
and one framebuffer push. Idle host tests cover
visible blink phases, expression geometry, mood-weighted choice, frame gating,
sleep/menu/B priority, delayed loop steps, rollover deadlines, no NVS writes,
and one complete OLED push per frame. For the pet state
addition, checks confirm that weather/time code, graphics, blinking, animation,
and the original button/sleep implementations remain intact except for the
explicit pet hooks. Host tests using temporary ESP32 API/storage stubs exercise
accepted/rejected interactions, temporary moods, sleep schedule boundaries,
friendship limits, RTC/NVS freshness selection, power-loss restoration, sleep
commits, write batching, legacy migration, checksum fallback, failed storage and
read-back, birthday preservation, and deferred birthday capture. A full module
compile/link check uses the same stubs. Hardware tests still need the existing device.

On the physical device:

1. Compile/upload with the existing board options and library versions. Keep
   the existing NVS partition; erasing flash erases pet checkpoints.
2. On HOME during awake hours, press B and verify the same hop/heart, plus one
   counted interaction in Serial Monitor. Rapid presses should count but award
   only one friendship point in ten minutes. Menus should remain unchanged.
3. Press B several times, note the count, then wait for the normal 30-second
   sleep and a `Pet NVS saved (sleep)` message. Unplug/reconnect and verify
   `Pet loaded from NVS` restores friendship, the exact count, and birthday.
4. Verify powered B wake reports `Pet loaded from RTC` with the same progress.
   Waking and sleeping without interaction should not produce another save.
   Rapid presses should not write per press; after ten minutes an interaction
   may earn one more friendship point and trigger a checkpoint. Birthday should
   remain identical across all reboots. Power loss before a checkpoint can roll
   back recent interaction-only changes.
5. Check 07:00 and 22:00 behavior: sleeping B presses should not react or count.
   Verify the same clock, sunrise/sunset, weather/moon graphics, blinking,
   animations, Wi-Fi behavior, cached-weather wake, and stale-cache refresh.
6. If starting with no valid clock, confirm birthday is initially 0 and is
   captured once after the normal clock becomes valid. No added sync is attempted.

## License

A license has not yet been selected; see `LICENSE`.
