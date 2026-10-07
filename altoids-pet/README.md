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
│       ├── weather.cpp / weather.h
│       ├── power.cpp / power.h
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
- `weather` owns Wi-Fi/NTP sync, timezone, local clock and moon calculations,
  Open-Meteo parsing, sunrise/sunset, weather mapping, and the RTC weather cache.
- `power` owns button pins, polling/debounce, activity tracking, wake-cause
  detection, and deep sleep.

State is defined once in its owning module. Headers expose shared state and
functions needed by the other modules. The cache fields and online sync
timestamp retain `RTC_DATA_ATTR`. Graphics remain the original drawing commands;
the other directories are reserved for project materials.

## Preserved behavior

Cold boot attempts Wi-Fi/NTP and Open-Meteo sync. Central timezone/DST is
restored on every boot. On B-button wake, the firmware restores time and weather
from RTC memory; it attempts an online refresh when the cache is missing or
its valid sync timestamp is more than three hours old. The original timestamp
checks and failure fallbacks remain intact. This is a wake-time cache check,
not a new periodic online refresh.

- A opens the menu from home and cycles its entries.
- B reacts with the pet while awake, opens WEATHER from the menu, or logs the
  selected TIMER, PET, or SETTINGS entry.
- C returns from weather to the menu, or otherwise returns home.
- Pet sleep remains from 22:00 until 07:00. Reaction duration, blinking,
  weather animations, moon phases, sunrise/sunset, and screen drawing are
  preserved.
- Clock checks remain every five seconds while time is valid, with clock-driven
  home redraws when the displayed minute changes.
- After 30 seconds without an accepted button press, the OLED turns off and the
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

## Validation

The original refactor was verified against `testsketch`. Idle host tests cover
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
compile/link check uses the same stubs. Real board compilation and hardware
tests still need the existing Arduino environment.

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
