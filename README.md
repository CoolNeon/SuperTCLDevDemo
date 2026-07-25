# SuperTCLDevDemo

Example code for the Total Control Lighting Developer Shield, compatible with
both the **Developer Shield** and **Simple Shield** from Cool Neon.  This
sketch combines several LED animation modes into a single program that, when
uploaded to an Arduino with a Cool Neon TCL shield, lets you switch between
modes and tweak parameters live via potentiometers.  It is the code
pre-loaded into the TCL dev modules, now ready for you to hack.

---

## Table of Contents

- [Hardware Requirements](#hardware-requirements)
- [LED Strip Configuration](#led-strip-configuration)
- [Quick Start](#quick-start)
- [Shield Detection](#shield-detection)
- [Mode Selection](#mode-selection)
- [Animation Modes](#animation-modes)
  - [1. Fire Strand](#1-fire-strand)
  - [2. Cylon Eye](#2-cylon-eye)
  - [3. Color Picker](#3-color-picker)
  - [4. Rain Bling](#4-rain-bling)
- [Adjusting Strand Length](#adjusting-strand-length)
- [Momentary Button Effects](#momentary-button-effects)
- [EEPROM Settings](#eeprom-settings)
- [Configurable Constants](#configurable-constants)

---

## Hardware Requirements

- An **Arduino** board (Uno, Mega, etc.)
- A **Cool Neon TCL Developer Shield** or **Simple Shield**
- An **LED strip** compatible with the chosen chipset (e.g. P9813, WS2812B, etc.)
- A power supply appropriate for your LED strip

The Developer Shield provides:
- 4 toggle-switch positions (2 switches → 4 modes)
- 2 momentary buttons
- 4 potentiometers (analog knobs)

---

## LED Strip Configuration

When swapping to a different LED strip type, change these two `#define` lines
near the top of `SuperTCLDevDemo.ino`:

| Define             | Default  | Description                                                      |
|--------------------|----------|------------------------------------------------------------------|
| `LED_CHIPSET`      | `P9813`  | FastLED chipset family. Common alternatives: `WS2812B`, `WS2811`, `NEOPIXEL`, `APA102`. |
| `LED_COLOR_ORDER`  | `RGB`    | Byte order your strip expects. Common alternatives: `GRB`, `BGR`, `RGB`.  Check your strip's datasheet. |

No other code changes are needed — data and clock pins are already provided by
the `CoolNeon_DevShield` library.

---

## Quick Start

1. Install the required libraries via the Arduino Library Manager:
   - **CoolNeon_DevShield**
   - **FastLED**
2. Connect your LED strip data pin to the Developer Shield's data/clock headers.
3. Select your strip's chipset and color order in the `#define` section (see above).
4. Set `MAXLEDS` to at least the number of LEDs in your strip.
5. Compile and upload to your Arduino.

---

## Shield Detection

On startup, the sketch samples the stability of `POT1` to determine whether a
**Developer Shield** (with all four pots and two DIP switches) or a **Simple
Shield** (with no switches and floating pots) is installed:

- **Developer Shield detected** → all four animation modes are selectable via
  the DIP switches, and all four potentiometers are active for real-time control.
- **Simple Shield detected** → the sketch defaults to **Rain Bling** mode with a
  fixed set of visually appealing presets.  If any switch or button changes are
  detected at runtime, the sketch upgrades to Developer Shield mode automatically.

---

## Mode Selection

On the Developer Shield, two DIP switches (`SWITCH1` and `SWITCH2`) select the
active animation mode:

| SWITCH1 | SWITCH2 | Mode          |
|---------|---------|---------------|
| OFF     | OFF     | Fire Strand   |
| OFF     | ON      | Cylon Eye     |
| ON      | OFF     | Color Picker  |
| ON      | ON      | Rain Bling    |

(On the Simple Shield, only Rain Bling is available.)

---

## Animation Modes

### 1. Fire Strand

A flickering fire effect that animates continuously across the strand.

| Potentiometer | Effect                                  |
|---------------|-----------------------------------------|
| POT1          | **Speed** — animation speed (faster as you turn up) |
| POT3          | **Warmth / Chromatography** — green content (higher = more green/yellow fire) |
| POT4          | **Intensity** — overall brightness/density of flame pixels (0–100%) |

Each frame, random red and green values are generated per pixel, scaled by
intensity and warmth, creating a natural-looking flame.

---

### 2. Cylon Eye

A back-and-forth scanner (Knight Rider / Cylon) effect with a trailing fade.
The active pixel sweeps from one end of the strand to the other and back.

| Potentiometer | Effect                                  |
|---------------|-----------------------------------------|
| POT1          | **Red** channel value                   |
| POT2          | **Green** channel value                 |
| POT3          | **Blue** channel value                  |
| POT4          | **Sweep delay** — 10 ms (fast) to 150 ms (slow) |

The trailing fade divides each trailing pixel's brightness by 2 per step,
leaving a 10-pixel comet tail behind the leading dot.  Trailing pixels beyond
the tail are cleared to black.

---

### 3. Color Picker

Shifts existing pixel colors down the strand by one position and injects the
current potentiometer color at the head, creating a continuous color stream.

| Potentiometer | Effect                                  |
|---------------|-----------------------------------------|
| POT1          | **Red** value injected at head          |
| POT2          | **Green** value injected at head        |
| POT3          | **Blue** value injected at head         |
| POT4          | **Speed** — animation speed (faster as you turn up) |

---

### 4. Rain Bling

An HSV-rainbow animation with random white "lightning" flashes scattered across
the strand.  The rainbow hue cycles continuously, producing a smooth color
gradient along the strand.

| Potentiometer | Effect                                  |
|---------------|-----------------------------------------|
| POT1          | **Speed** — hue rotation speed (faster as you turn up) |
| POT2          | **Brightness** — overall value (0–99%)  |
| POT3          | **Saturation** — color intensity (0–100%) |
| POT4          | **Flash probability** — more white lightning strikes as you turn up |

Gamma correction is applied to the output for more natural color rendering.

---

## Adjusting Strand Length

You can dynamically change the number of active LEDs without recompiling:

1. **Hold down `MOMENTARY2`**.
2. While holding it, **turn `POT2`** (lower-right analog pot).
3. Red pixels show the active region; a single blue pixel marks the end.
4. Release `MOMENTARY2` — the new length is saved to **EEPROM** automatically.

The value is restored on the next power cycle, so you only need to set it once.

---

## Momentary Button Effects

| Button       | Effect                                                             |
|--------------|--------------------------------------------------------------------|
| `MOMENTARY1` | While held in **Fire Strand** mode, swaps the red/green/blue channels.  In all other modes, inverts the color output (bitwise NOT). |
| `MOMENTARY2` | While held, enters strand-length edit mode (see above).            |

---

## EEPROM Settings

The active strand length is stored in EEPROM at address 0 using a custom struct
with a checksum for integrity:

```
struct SettingsObject {
  int NumberOfLEDs;   // 1..MAXLEDS, clamped on read and write
  int checksum;       // (NumberOfLEDs + 69) * 42
};
```

- **On read**: if the checksum matches, the stored value is used (clamped to
  valid range).  If the checksum is invalid, the default is written and used.
- **On write**: no write occurs if the stored value and checksum already match,
  reducing EEPROM wear.

---

## Configurable Constants

| Constant         | Default | Description                                                     |
|------------------|---------|-----------------------------------------------------------------|
| `MAXLEDS`        | 100     | Maximum number of LEDs the pixel buffer can address.            |
| `ACTIVELEDS`     | 100     | Active strand length; adjustable at runtime via MOMENTARY2+POT2. |
| `DELAYLOW`       | 10      | Minimum delay (ms) used in cylon_eye sweep.                    |
| `DELAYHIGH`      | 150     | Maximum delay (ms) used in cylon_eye sweep.                    |
| `LED_CHIPSET`    | P9813   | FastLED chipset family (change when swapping strip types).      |
| `LED_COLOR_ORDER`| RGB     | FastLED color byte order (change when swapping strip types).    |

---

## Version History

### 2.0.1 — Performance & memory optimization

**SRAM: ~610 bytes → ~354 bytes (40% reduction)**

- Moved the 256-byte gamma-correction lookup table from SRAM to flash (PROGMEM),
  freeing a substantial chunk of the Uno's 2 KB SRAM for future features.
- Inlined `transformPixel()` into `update_strand()` and read the MOMENTARY1
  button state once per frame instead of once per pixel, eliminating ~100
  `digitalRead()` calls per frame.
- Replaced integer division (`/ 2`) with single-cycle bit shifts (`>>= 1`) in
  the Cylon Eye trailing-fade computation.
- Replaced temporary `CRGB(r, g, b)` constructors with `.setRGB()` and direct
  member assignment in all hot paths (FireStrand, cylon_eye, CheckSwitches).
- Lifted `check_color_pots()` out of the cylon_eye per-pixel loop — potentiometer
  values are now sampled once per sweep rather than once per pixel, saving
  hundreds of `analogRead()` + `map()` calls per frame.
- Removed dead code: the standalone `transformPixel()` function and
  commented-out test routines in `setup()`.

### 2.0.0 — FastLED migration

- Replaced arduino-tcl with CoolNeon_DevShield for shield inputs.
- Replaced TCL output calls with FastLED (P9813 data/clock output).
- Added GitHub Actions CI: Arduino Lint + compile check on every PR.
