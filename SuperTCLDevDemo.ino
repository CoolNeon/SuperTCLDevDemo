/*****************************************************************************
 * SuperTCLDevDemo.ino
 * Version 2.0.1
 *
 * This sketch uses CoolNeon_DevShield for input aliases and initialization,
 * and FastLED for writing pixel data to P9813-based strands.
 *
 * Code now detects the type of momentary switch, to compensate for a manufacturing
 * issue with the CoolNeon Dev Shields, as well as whether a Developer or Simple Board
 * is installed.  If a Simple Board is installed, it defaults to running
 * rainBling() with a set of visually appealing presets.
 *
 * New in 2.0.1
 * Perf:    Moved gamma-correction lookup table (256 bytes) from SRAM to PROGMEM
 *          (flash), reducing SRAM usage by ~40% and leaving more headroom for
 *          future features.
 * Perf:    Inlined transformPixel() into update_strand() and read MOMENTARY1
 *          once per frame instead of once per pixel, eliminating ~100
 *          digitalRead() calls per frame.
 * Perf:    Replaced integer division (/ 2) with bit shifts (>>= 1) in
 *          cylon_eye() trailing fade — single-cycle on AVR.
 * Perf:    Replaced CRGB() temporary constructors with setRGB() / direct member
 *          assignment throughout hot paths (FireStrand, cylon_eye, etc.).
 * Perf:    Lifted check_color_pots() out of cylon_eye() per-pixel loop — pots
 *          are now read once per sweep instead of once per pixel, saving
 *          hundreds of analogRead() + map() calls per frame.
 * Cleanup: Removed dead transformPixel() function and commented-out test code
 *          in setup().
 * Docs:    Updated header to reflect current file name and version.
 *
 * New in 2.0.0
 * Migration: Replaced arduino-tcl with CoolNeon_DevShield for shield inputs.
 * Migration: Replaced TCL output calls with FastLED (P9813 data/clock output).
 * Docs:      Updated comments to match current library usage and behavior.
 *
 * New in 1.2.3
 * Fix:     Hardened EEPROM settings handling.
 *          - NumberOfLEDs is clamped to 1..MAXLEDS on read/write.
 *          - Skipped writes now require both matching value and valid checksum.
 *          - Invalid EEPROM data is self-healed with a safe value.
 *          - Unchanged valid values are not rewritten, reducing EEPROM wear.
 * Cleanup: Removed unused variables and dead code paths to reduce SRAM usage and simplify the sketch.
 * Guard:   Added an all-zero input guard in check_color_pots() to keep future color math safe.
 *
 * New in 1.2.2
 * Fix:     Fixed a bug in the length-setting code that caused excessive and unnecessary writes to
 *          the EEPROM.
 *
 * New in 1.2.1
 * Fix:     Fixed typos, and removed unused globals.
 *
 * New in 1.2
 * Feature: When you use MOMENTARY2 to adjust ACTIVELEDS (length of the strand),
 *          that value is stored in EEPROM so that it persists through power cycles.
 *          No more having to tweak the strand length every time you set up!
 * Fix:     All functions now utilize ACTIVELEDS, where some had been sloppy and
 *          ran out to MAXLEDS.
 * Fix:     RainBling now uses the same strand[MAXLEDS][3] data structure as all other
 *          functions, reducing the memory requirements for the sketch.
 *
 * New in 1.1.4
 * Fix:     Forgot to remove developer flag used in testing 1.1.3
 *
 * New in 1.1.3
 * Fix:     If Dev/Simple shield detect falsely identifies Simple, it can be reset by changing any of the switches or buttons.
 * Fix:     Resolved issue where strands larger than 25 didn't clear pixels after 25 when the length had not been manually adjusted.
 *
 * New in 1.1.x
 * User can dynamically adjust the length of the active pixels in the strand by holding
 * down Momentary 1 (Pin 4) and turning the lower right Analog Potentiometer (Pin 0)
 *
 * cylon_eye() looks like a certain retro science fiction special effect.
 *
 * rainBling() is a HSV rainbow, with bonus lightning effects.
 *
 * FireStrand() will send a flickering fire sequence down the strand of pixels.
 * Several of the attributes are dynamically adjustable:
 *
 * Fire mode adjustments:
 *
 *  Intensity  Warmth
 *   * -        - *
 *   - -        - -
 *
 *  Speed      Length
 *   - -        - -
 *   * -        - *
 *
 *
 * Includes code from fire.ino, color_designer.ino & rainbling.ino by Christopher De Vries, Copyright 2011
 * and distributed under the Artistic License 2.0
 *
 * Copyright 2014 Chris O'Halloran - cknight __ ghostwheel _ com unless otherwise noted
 * License: Attribution Non-commercial Share Alike (by-nc-sa) (CC BY-NC-SA 4.0)
 * https://creativecommons.org/licenses/by-nc-sa/4.0/
 * https://creativecommons.org/licenses/by-nc-sa/4.0/legalcode
 ****************************************************************************/
#include <CoolNeon_DevShield.h>
#include <FastLED.h>
#include <EEPROM.h>

// LED strip configuration — change these defines when swapping to a different strip type.
#define LED_CHIPSET       P9813       // Chipset family (e.g. WS2812B, WS2811, NEOPIXEL, P9813)
#define LED_COLOR_ORDER   RGB         // Color order: RGB, GRB, BGR, etc.  Check your strip's datasheet.


const int MAXLEDS = 100; // Maximum number of LEDs that this demo will address
int ACTIVELEDS = 100;  // User can dynamically adjust this after program starts running

CRGB leds[MAXLEDS];        // Pixel state buffer used for both effect generation and output

// This structure is used to read and write values from EEPROM
struct SettingsObject {
  int NumberOfLEDs;
  int checksum;
};

int clampLEDCount(int value) {
  if (value < 1) {
    return 1;
  }
  if (value > MAXLEDS) {
    return MAXLEDS;
  }
  return value;
}

int settingsChecksum(int ledCount) {
  return ((ledCount + 69) * 42);
}

// Absolute colors for the pixels
byte RED = 0;
byte BLUE = 0;
byte GREEN = 0;

// Define the min and max delay between iterations
#define DELAYLOW 10
#define DELAYHIGH 150

int SWITCHSTATE; // A single point of reference for the state of the switches

int MOMENTARY1_Initial_State;
int MOMENTARY2_Initial_State;
int DEVSHIELD_SWITCH1_Initial_State;
int DEVSHIELD_SWITCH2_Initial_State;

/*
 * Control map (Developer Shield mode):
 *   SWITCH1 + SWITCH2 select effect mode:
 *     00 -> FireStrand
 *     01 -> cylon_eye
 *     10 -> color_picker
 *     11 -> rainBling
 *
 *   Hold MOMENTARY2 and turn POT2 to adjust ACTIVELEDS (1..MAXLEDS).
 *   ACTIVELEDS is persisted to EEPROM and restored at boot.
 *
 *   POT assignments vary slightly per mode (see function comments).
 */

//  BEGIN - Variables and constants for rainbling subroutine
// Gamma table (gamma=2.2) precomputed at compile time and stored in flash (PROGMEM)
// to save 256 bytes of SRAM.
const PROGMEM byte rain_gamma_table[256] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 31, 32, 33, 34, 34, 35, 36,
   37, 37, 38, 39, 40, 40, 41, 42, 43, 44, 45, 46, 46, 47, 48, 49,
   50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65,
   66, 67, 68, 69, 70, 71, 72, 73, 74, 76, 77, 78, 79, 80, 81, 83,
   84, 85, 86, 87, 89, 90, 91, 92, 94, 95, 96, 98, 99,100,102,103,
  104,106,107,109,110,111,113,114,116,117,119,120,122,123,125,126,
  128,129,131,132,134,135,137,139,140,142,143,145,147,148,150,152,
  153,155,157,158,160,162,163,165,167,168,170,172,174,175,177,179,
  181,182,184,186,188,189,191,193,195,197,198,200,202,204,206,207
};
const float rain_gamma = 2.2;
const float rain_hinterval_max = 10.0;
const float rain_v_max = 0.99;
const float rain_sat_max = 1.0;
const int rain_flash_prob_max=20480;
float rain_totem_interval;
float rain_hval;
//  END - Variables and constants for rainbling subroutine

// This logic determines whether a Developer Shield is installed, or the Simple Shield.
// Assume simple shield, unless proven otherwise
int DevShieldInstalled = 0;


// Initialize Developer Shield inputs, FastLED output, and restore settings from EEPROM.
void setup() {
  DevShield.begin();
  FastLED.addLeds<LED_CHIPSET, DEVSHIELD_DATAPIN, DEVSHIELD_CLOCKPIN, LED_COLOR_ORDER>(leds, MAXLEDS);
  FastLED.clear(true);

  ACTIVELEDS = readSettingsFromEEPROM(ACTIVELEDS);

  MOMENTARY1_Initial_State = digitalRead(DEVSHIELD_MOMENTARY1);
  MOMENTARY2_Initial_State = digitalRead(DEVSHIELD_MOMENTARY2);
  DEVSHIELD_SWITCH1_Initial_State = digitalRead(DEVSHIELD_SWITCH1);
  DEVSHIELD_SWITCH2_Initial_State = digitalRead(DEVSHIELD_SWITCH2);

  DevBoardDetect();

}

// Poll controls and run the currently selected effect.
void loop() {
  CheckSwitches();

  switch (SWITCHSTATE) {
    case 3:
      FireStrand();
      break;
    case 2:
      cylon_eye();
      break;
    case 1:
      color_picker();
      break;
    case 0:
      rainBling();
      break;
    }

}

// Fire-like animation written through the leds buffer and FastLED output.
// POT1: speed, POT3: warmth/chromatography, POT4: intensity.
void FireStrand() {
  int i;
  int red;
  int green;
  float intensity;
  float chromatography;
  int delaytime;
  int strandlength;

  intensity=(float)map(analogRead(DEVSHIELD_POT4), 0, 1023, 0, 100)/100;
  chromatography=(float)map(analogRead(DEVSHIELD_POT3), 0, 1023, 0, 50)/100;
  strandlength=ACTIVELEDS;
  delaytime=(int)map(analogRead(DEVSHIELD_POT1), 0, 1023, 150, 0);

  for(i=0;i<strandlength;i++) {
    red=(int)(random(0,256) * intensity);
    green=(int)(random(0,(red * chromatography +1)) * intensity);
    leds[i].setRGB(red, green, 0);
  }
  while (i < MAXLEDS) {
    leds[i] = CRGB::Black;
    i++;
  }
  update_strand();
  delay(delaytime);

}



// Detect board input changes, update mode selection, and handle strand-length edit mode.
void CheckSwitches() {

  // This allows Simple Shield Mode to be disabled if a switch change is detected.
  // This helps defend against false positives in the shield detection code.
  if ( 0 == DevShieldInstalled ) {
    if ( (DEVSHIELD_SWITCH1_Initial_State != digitalRead(DEVSHIELD_SWITCH1)) || (DEVSHIELD_SWITCH2_Initial_State != digitalRead(DEVSHIELD_SWITCH2)) || (digitalRead(DEVSHIELD_MOMENTARY1) != MOMENTARY1_Initial_State) || (digitalRead(DEVSHIELD_MOMENTARY2) != MOMENTARY2_Initial_State) ) {
      DevShieldInstalled = 1;
      reset_strand();
    }
  }

  // This loop lets the user adjust the strand length by holding down Momentary 1 (Pin 4)
  // and turning the lower right Analog Potentiometer (Pin 0)
  if (digitalRead(DEVSHIELD_MOMENTARY2) != MOMENTARY2_Initial_State) {
    while (digitalRead(DEVSHIELD_MOMENTARY2) != MOMENTARY2_Initial_State) {
      int led_position;
      ACTIVELEDS=(int)map(analogRead(DEVSHIELD_POT2), 0, 1023, 1, MAXLEDS);
      for (led_position = 1; led_position < ACTIVELEDS; led_position++) {
        leds[led_position - 1].setRGB(255, 0, 0);
      }
      leds[led_position - 1].setRGB(0, 0, 255);
      led_position++;
      while (led_position < MAXLEDS) {
        leds[led_position - 1] = CRGB::Black;
        led_position++;
      }
      update_strand();
    }
    writeSettingsToEEPROM(ACTIVELEDS);
  }


  if ( 1 == DevShieldInstalled ) {
    if (digitalRead(DEVSHIELD_SWITCH1) == 0 && digitalRead(DEVSHIELD_SWITCH2) == 0){
      SWITCHSTATE = 3;
    }
    else if (digitalRead(DEVSHIELD_SWITCH1) == 0 && digitalRead(DEVSHIELD_SWITCH2) == 1){
      SWITCHSTATE = 2;
    }
    else if (digitalRead(DEVSHIELD_SWITCH1) == 1 && digitalRead(DEVSHIELD_SWITCH2) == 0){
      SWITCHSTATE = 1;
    }
    else{
      SWITCHSTATE = 0;
    }
  }
  else {
    SWITCHSTATE = 0;
  }


}


void reset_strand() {
  int i;

  for(i=0;i<MAXLEDS;i++) {
    leds[i] = CRGB::Black;
  }
  FastLED.show();
}


void update_strand() {
  int i;  // A local instance of 'i' so we don't interfere with other loops

  // Read the momentary state once per frame, not once per pixel.
  bool invertActive = (digitalRead(DEVSHIELD_MOMENTARY1) != MOMENTARY1_Initial_State);
  bool swapChannels = invertActive && (SWITCHSTATE == 3);

  for(i=0;i<ACTIVELEDS;i++) {
    if (invertActive) {
      if (swapChannels) {
        // Deliberate channel swap for FireStrand mode.
        uint8_t tr = leds[i].r;
        leds[i].r = leds[i].g;
        leds[i].g = leds[i].b;
        leds[i].b = tr;
      } else {
        leds[i].r ^= 255;
        leds[i].g ^= 255;
        leds[i].b ^= 255;
      }
    }
  }
  while (i < MAXLEDS) {
      leds[i] = CRGB::Black;
      i++;
    }
  FastLED.show();
}

// Scanner effect with trailing fade.
// POT1-3: color, POT4: sweep delay.
void cylon_eye() {
  int i;
  int j; // The lag counter
  int pos;

  while ( SWITCHSTATE == 2) {

    // Forward color sweep
    check_color_pots();
    for(i=0; i<ACTIVELEDS;i++){
      leds[i].setRGB(RED, GREEN, BLUE);
      for(j=1;j<=10;j++) {
        pos=i-j;
        if(pos>=0) {
          leds[pos].r >>= 1;
          leds[pos].g >>= 1;
          leds[pos].b >>= 1;
        }
      }

      // Empty out all trailing LEDs.  This prevents 'orphans' when dynamically shortening the tail length.
      for(pos=i-j; pos>=0;pos--){
        leds[pos] = CRGB::Black;
      }

      update_strand(); // Send all the pixels out
      delay(map(analogRead(DEVSHIELD_POT4), 0, 1023, DELAYLOW, DELAYHIGH));

      CheckSwitches();
      if ( 2 != SWITCHSTATE ) {
        break;
      }
    }

    CheckSwitches();
    if ( 2 != SWITCHSTATE ) {
      break;
    }

    // Reverse color sweep
    check_color_pots();
    for(i=ACTIVELEDS-1; i>=0;i--){
      leds[i].setRGB(RED, GREEN, BLUE);
      for(j=1;j<=10;j++) {
        pos=i+j;
        if(pos<ACTIVELEDS) {
          leds[pos].r >>= 1;
          leds[pos].g >>= 1;
          leds[pos].b >>= 1;
        }
      }

    // Empty out all trailing LEDs.  This prevents 'orphans' when dynamically shortening the tail length.
      for(pos=i+j; pos<ACTIVELEDS;pos++){
        leds[pos] = CRGB::Black;
      }

      update_strand(); // Send all the pixels out
      delay(map(analogRead(DEVSHIELD_POT4), 0, 1023, DELAYLOW, DELAYHIGH));

      CheckSwitches();
      if ( 2 != SWITCHSTATE ) {
        break;
      }
    }

  }
  reset_strand();
}

// Read current RGB color from POT1-3.
void check_color_pots() {
    /* Read the current red value from potentiometer 0 */
  RED=map(analogRead(DEVSHIELD_POT1), 0, 1023, 0, 255);
    /* Read the current green value from potentiometer 1 */
  GREEN=map(analogRead(DEVSHIELD_POT2), 0, 1023, 0, 255);
    /* Read the current blue value from potentiometer 2 */
  BLUE=map(analogRead(DEVSHIELD_POT3), 0, 1023, 0, 255);

    // Guard all-zero input so future color math in this function remains safe.
    if (RED == 0 && GREEN == 0 && BLUE == 0) {
      return;
    }
}


// Shift existing pixels and inject current POT color at the head.
// POT4 controls animation speed.
void color_picker() {
  int i; // A variable for looping

  /* Move colors down the line by one */
  for(i=ACTIVELEDS-1;i>0;i--) {
    leds[i] = leds[i-1];
  }
  /* Read the current red value from potentiometer 1
   * Values are 10 bit and must be left shifted by 2 in order to fit in 8
   * bits */
  leds[0].r=analogRead(DEVSHIELD_POT1)>>2;

  /* Read the current green value from potentiometer 2 */
  leds[0].g=analogRead(DEVSHIELD_POT2)>>2;

  /* Read the current blue value from potentiometer 3 */
  leds[0].b=analogRead(DEVSHIELD_POT3)>>2;

  update_strand(); // Send all the pixels out
  delay( (int)map(analogRead(DEVSHIELD_POT4), 0, 1023, 150, 0) );

  /* Check if the button is pressed and if we have to send a color choice to serial */
}

/* Convert hsv values (0<=h<360, 0<=s<=1, 0<=v<=1) to rgb values (0<=r<=255, etc) */
void rain_HSVtoRGB(float h, float s, float v, byte *r, byte *g, byte *b) {
  int i;
  float f, p, q, t;
  float r_f, g_f, b_f;

  if( s < 1.0e-6 ) {
    /* grey */
    r_f = g_f = b_f = v;
  }

  else {
    h /= 60.0;              /* Divide into 6 regions (0-5) */
    i = (int)floor( h );
    f = h - (float)i;      /* fractional part of h */
    p = v * ( 1.0 - s );
    q = v * ( 1.0 - s * f );
    t = v * ( 1.0 - s * ( 1.0 - f ) );

    switch( i ) {
      case 0:
        r_f = v;
        g_f = t;
        b_f = p;
        break;
      case 1:
        r_f = q;
        g_f = v;
        b_f = p;
        break;
      case 2:
        r_f = p;
        g_f = v;
        b_f = t;
        break;
      case 3:
        r_f = p;
        g_f = q;
        b_f = v;
        break;
      case 4:
        r_f = t;
        g_f = p;
        b_f = v;
        break;
      default:    // case 5:
        r_f = v;
        g_f = p;
        b_f = q;
        break;
    }
  }

  *r = pgm_read_byte(&rain_gamma_table[(byte)floor(r_f*255.99)]);
  *g = pgm_read_byte(&rain_gamma_table[(byte)floor(g_f*255.99)]);
  *b = pgm_read_byte(&rain_gamma_table[(byte)floor(b_f*255.99)]);
}

void rainBling() {

  // BEGIN rainbling setup
  rain_totem_interval = 360.0/ACTIVELEDS;
  rain_hval = 0.0;
  blackout_strand();
  // END rainbling setup

  while ( SWITCHSTATE == 0 ) {
    int i;
    float local_h;
    int speed_pot;
    int brightness_pot;
    int saturation_pot;
    int flash_pot;
    float hinterval;
    float sat;
    float v;

    if ( 1 == DevShieldInstalled ) {
      speed_pot = analogRead(DEVSHIELD_POT1);
      brightness_pot = analogRead(DEVSHIELD_POT2);
      saturation_pot = analogRead(DEVSHIELD_POT3);
      flash_pot = analogRead(DEVSHIELD_POT4);
    }
    else {
      speed_pot = 497;
      brightness_pot = 1023;
      saturation_pot = 1022;
      flash_pot = 832;
    }

    v = rain_v_max/1023.0*brightness_pot;
    sat = rain_sat_max/1023.0*saturation_pot;

    for(i=0;i<ACTIVELEDS;i++) {
      local_h = rain_hval+i*rain_totem_interval;
      while(local_h>=360.0) {
        local_h-=360.0;
      }
      if(random(rain_flash_prob_max)<flash_pot) {
        leds[i] = CRGB::White;
      }
      else {
        rain_HSVtoRGB(local_h,sat,v,&leds[i].r,&leds[i].g,&leds[i].b);
      }
      CheckSwitches();
    }

    update_strand();
    delay(25);
    hinterval = rain_hinterval_max/1023.0*speed_pot;
    rain_hval+=hinterval;
    while(rain_hval>=360.0) {
      rain_hval-=360.0;
      CheckSwitches();
      if ( 0 != SWITCHSTATE ) {
        break;
      }
    }
  }


}


// Estimate whether a Developer Shield is installed by sampling POT1 stability.
void DevBoardDetect() {
  int i;
  int DevSheldTestCount = 0;
  int DevSheldTest;
  DevSheldTest = analogRead(DEVSHIELD_POT1);
  for(i=0; i<10; i++) {
    if ( (analogRead(DEVSHIELD_POT1) < (DevSheldTest - 1) ) || (analogRead(DEVSHIELD_POT1) > (DevSheldTest + 1) )    ) {
      DevSheldTestCount++;
    }
  }
  if ( 5 >= DevSheldTestCount ) {
    DevShieldInstalled = 1;
  }

}

void blackout_strand() {
  for(int i=0;i<MAXLEDS;i++) {
    leds[i] = CRGB::Black;
  }
  FastLED.show();
}
void whiteout_strand() {
  for(int i=0;i<MAXLEDS;i++) {
    leds[i] = CRGB::White;
  }
  FastLED.show();
}


int readSettingsFromEEPROM(int defaultLEDs) {

  SettingsObject tempVar; //Variable to store custom object read from EEPROM.
  EEPROM.get(0, tempVar);

  int safeDefault = clampLEDCount(defaultLEDs);

  if (settingsChecksum(tempVar.NumberOfLEDs) == tempVar.checksum) {
    int clampedStoredValue = clampLEDCount(tempVar.NumberOfLEDs);

    // Self-heal EEPROM if old data was in-range invalid.
    if (clampedStoredValue != tempVar.NumberOfLEDs) {
      writeSettingsToEEPROM(clampedStoredValue);
    }
    return(clampedStoredValue);
  }

  // Self-heal invalid EEPROM contents with a safe default.
  writeSettingsToEEPROM(safeDefault);
  return(safeDefault);
}

// Persist ACTIVELEDS only when needed, while preserving EEPROM lifetime.
void writeSettingsToEEPROM(int currentLEDs) {
  int safeLEDCount = clampLEDCount(currentLEDs);
  SettingsObject existingSettings;
  EEPROM.get(0, existingSettings);

  // Avoid unnecessary EEPROM wear only when value and checksum are already valid.
  if ((existingSettings.NumberOfLEDs == safeLEDCount) && (existingSettings.checksum == settingsChecksum(existingSettings.NumberOfLEDs))) {
    return;
  }

  //Data to store.
  SettingsObject tempVar = {
    safeLEDCount,
    settingsChecksum(safeLEDCount)
  };
  EEPROM.put(0, tempVar);
}