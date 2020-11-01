/*****************************************************************************
 * SuperTCLdevDemo.ino
 * Version 1.2.1
 *
 * Notes:  Needs better comments and docs!
 *
 * Code now detects for type of momentary switch, to compensate for a manufacturing
 * issue with the TCL Dev Shields, as well as whether a TCL Developer or Simple Board
 * is installed.  If a TCL SImple Board is installed, it defaults to running
 * rainbling() with a set of visually appealing presets.
 *
 * New in 1.2
 * Feature: When you use TCL_MOMENTARY2 to adjust ACTIVELEDS (length of the strand),
 *          that value is stored in EEPROM so that it persists through power cycles.
 *          No more having to tweak the strand length every time you set up!
 * Fix:     All functions now utilize ACTIVELEDS, where some had been sloppy and
 *          ran out to MAXLEDS.
 * FIX:     RainBling now uses the same strand[MAXLEDS][3] data structure as all other
 *          functions, reducing the memory requirements for the sketch.
 *
 * New in 1.1.4
 * fix:  Forgot to remove developer flag used in testing 1.1.3
 *
 * New in 1.1.3
 * Fix:  If Dev/Simple shield detect falsely identifies Simple, it can be reset by changing any of the switches or buttons.
 * Fix:  Resolved issue where strands larger than 25 didn't clear pixels after 25 when the length had not been manually adjusted.
 *
 * New in 1.1.x
 * User can dymacially adjust the length of the active pixels in the strand by holding
 * down Momentary 1 (Pin 4) and turning the lower right Analog Potentiometer (Pin 0)
 *
 * cylon_eye() looks like a certain retro science fiction special effect.
 *
 * rainBling() is a HSV rainbow, with bonus lightning effects.
 *
 * FireStrand() will send a flickering fire sequence down the strand of TCL pixels.
 * Several of the attributes are dunamically adjustable:
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
#include <SPI.h>
#include <TCL.h>
#include <EEPROM.h>


const int MAXLEDS = 400; // Maximum number of LEDs that this demo will address
int ACTIVELEDS = 200;  // User can dymamically adjust this after program starts running

byte strand[MAXLEDS][3]; // 0=R, 1=G, 2=B

// This structure is used to read and write values from EEPROM
struct SettingsObject {
  int NumberOfLEDs;
  int checksum;
};

// Absolute colors for the pixels
byte RED = 0;
byte BLUE = 0;
byte GREEN = 0;
float ratioRED;
float ratioBLUE;
float ratioGREEN;
int colorSUM;
float ratioHIGHEST;

// Define the min and max delay between iterations
#define DELAYLOW 10
#define DELAYHIGH 150

int SWITCHSTATE; // A single point of reference for the state of the switches

int MOMENTARY1_Initial_State;
int MOMENTARY2_Initial_State;
int TCL_SWITCH1_Initial_State;
int TCL_SWITCH2_Initial_State;

//  BEGIN - Variables and constants for rainbling subroutine
byte rain_gamma_table[256];
const float rain_gamma = 2.2;
const float rain_hinterval_max = 10.0;
const float rain_v_max = 0.99;
const float rain_sat_max = 1.0;
const int rain_flash_prob_max=20480;
float rain_totem_interval;
float rain_hval;
//  END - Variables and constants for rainbling subroutine

// This is for new code that tries to determine whether the TCL Developer Shield is installed, or the Simple Shield
// Assume simple shield, unless proven otherwise
int DevSheildInstalled = 0;


void setup() {
  TCL.begin();
  TCL.setupDeveloperShield();

  ACTIVELEDS = readSettingsFromEEPROM(ACTIVELEDS);

  MOMENTARY1_Initial_State = digitalRead(TCL_MOMENTARY1);
  MOMENTARY2_Initial_State = digitalRead(TCL_MOMENTARY2);
  TCL_SWITCH1_Initial_State = digitalRead(TCL_SWITCH1);
  TCL_SWITCH2_Initial_State = digitalRead(TCL_SWITCH2);

  DevBoardDetect();
//  whiteout_strand();
//  delay(1000);
//  blackout_strand();

  for(int i=0;i<256;i++) {
    rain_gamma_table[i] = (byte)(pow(i/255.0,rain_gamma)*255.0+0.5);
  }

}

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

void FireStrand() {
  int i;
  int red;
  int green;
  float intensity;
  float chromatography;
  int delaytime;
  int strandlength;

  intensity=(float)map(analogRead(TCL_POT4), 0, 1023, 0, 100)/100;
  chromatography=(float)map(analogRead(TCL_POT3), 0, 1023, 0, 50)/100;
  strandlength=ACTIVELEDS;
  delaytime=(int)map(analogRead(TCL_POT1), 0, 1023, 150, 0);

  TCL.sendEmptyFrame();
  for(i=0;i<strandlength;i++) {
    red=(int)(random(0,256) * intensity);
    green=(int)(random(0,(red * chromatography +1)) * intensity);
    sendPixelData(red,green,0);
  }
  while (i < MAXLEDS) {
    TCL.sendColor(0,0,0);
    i++;
  }
  TCL.sendEmptyFrame();
  delay(delaytime);

}


void sendPixelData( int red, int green, int blue) {

  if (digitalRead(TCL_MOMENTARY1) != MOMENTARY1_Initial_State) {
    if ( 3 == SWITCHSTATE ) {
      TCL.sendColor(green, blue, red);  // The colors are a LIE!  On this line ONLY they are deliberately swapped!
    }
    else {
      TCL.sendColor((red ^ 255), (green ^ 255), (blue ^ 255));
    }
  }
  else {
    TCL.sendColor(red, green, blue);
  }
}



void CheckSwitches() {

  // This alows Simple Shield Mode to be disabled if a switch change is detected.
  // This helps defend against false positives in the shield detection code.
  if ( 0 == DevSheildInstalled ) {
    if ( (TCL_SWITCH1_Initial_State != digitalRead(TCL_SWITCH1)) || (TCL_SWITCH2_Initial_State != digitalRead(TCL_SWITCH2)) || (digitalRead(TCL_MOMENTARY1) != MOMENTARY1_Initial_State) || (digitalRead(TCL_MOMENTARY2) != MOMENTARY2_Initial_State) ) {
      DevSheildInstalled = 1;
      reset_strand();
    }
  }

  // This loop lets the user adjust the strand length by holding down Momentary 1 (Pin 4)
  // and turning the lower right Analog Potentiometer (Pin 0)
  if (digitalRead(TCL_MOMENTARY2) != MOMENTARY2_Initial_State) {
    while (digitalRead(TCL_MOMENTARY2) != MOMENTARY2_Initial_State) {
      int led_posiiton;
      ACTIVELEDS=(int)map(analogRead(TCL_POT2), 0, 1023, 1, MAXLEDS);
      TCL.sendEmptyFrame();
      for (led_posiiton = 1; led_posiiton < ACTIVELEDS; led_posiiton++) {
        TCL.sendColor(255,0,0);
      }
      TCL.sendColor(0,0,255);
      led_posiiton++;
      while (led_posiiton < MAXLEDS) {
        TCL.sendColor(0,0,0);
        led_posiiton++;
      }
      TCL.sendEmptyFrame();
    }
    writeSettingsToEEPROM(ACTIVELEDS);
  }


  if ( 1 == DevSheildInstalled ) {
    if (digitalRead(TCL_SWITCH1) == 0 && digitalRead(TCL_SWITCH2) == 0){
      SWITCHSTATE = 3;
    }
    else if (digitalRead(TCL_SWITCH1) == 0 && digitalRead(TCL_SWITCH2) == 1){
      SWITCHSTATE = 2;
    }
    else if (digitalRead(TCL_SWITCH1) == 1 && digitalRead(TCL_SWITCH2) == 0){
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
    strand[i][0]=0;  // R
    strand[i][1]=0;  // G
    strand[i][2]=0;  // B
  }
  update_strand();
}


void update_strand() {
  int i;  // A local instance of 'i' so we don't interfere with other loops

  TCL.sendEmptyFrame();
  for(i=0;i<ACTIVELEDS;i++) {
    sendPixelData(strand[i][0],strand[i][1],strand[i][2]);
  }
  while (i < MAXLEDS) {
      TCL.sendColor(0,0,0);
      i++;
    }
  TCL.sendEmptyFrame();
}

void cylon_eye() {
  int i;
  int j; // The lag counter
  int pos;

  while ( SWITCHSTATE == 2) {

    // Forward color sweep
    for(i=0; i<ACTIVELEDS;i++){
      check_color_pots();
      strand[i][0]=RED;
      strand[i][1]=GREEN;
      strand[i][2]=BLUE;
      for(j=1;j<=10;j++) {
        pos=i-j;
        if(pos>=0) {
          strand[pos][0] = strand[pos][0] / 2;
          strand[pos][1] = strand[pos][1] / 2;
          strand[pos][2] = strand[pos][2] / 2;
        }
      }

      // Empty out all trailing LEDs.  This prevents 'orphans' when dynamically shortening the tail length.
      for(pos=i-j; pos>=0;pos--){
        strand[pos][0]=0;
        strand[pos][1]=0;
        strand[pos][2]=0;
      }

      update_strand(); // Send all the pixels out
      delay(map(analogRead(TCL_POT4), 0, 1023, DELAYLOW, DELAYHIGH));

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
    for(i=ACTIVELEDS-1; i>=0;i--){
      check_color_pots();
      strand[i][0]=RED;
      strand[i][1]=GREEN;
      strand[i][2]=BLUE;
      for(j=1;j<=10;j++) {
        pos=i+j;
        if(pos<ACTIVELEDS) {
          strand[pos][0] = strand[pos][0] / 2;
          strand[pos][1] = strand[pos][1] / 2;
          strand[pos][2] = strand[pos][2] / 2;
        }
      }

    // Empty out all trailing LEDs.  This prevents 'orphans' when dynamically shortening the tail length.
      for(pos=i+j; pos<ACTIVELEDS;pos++){
        strand[pos][0]=0;
        strand[pos][1]=0;
        strand[pos][2]=0;
      }

      update_strand(); // Send all the pixels out
      delay(map(analogRead(TCL_POT4), 0, 1023, DELAYLOW, DELAYHIGH));

      CheckSwitches();
      if ( 2 != SWITCHSTATE ) {
        break;
      }
    }

  }
  reset_strand();
}

void check_color_pots() {
    /* Read the current red value from potentiometer 0 */
    RED=map(analogRead(TCL_POT1), 0, 1023, 0, 255);
    /* Read the current green value from potentiometer 1 */
    GREEN=map(analogRead(TCL_POT2), 0, 1023, 0, 255);
    /* Read the current blue value from potentiometer 2 */
    BLUE=map(analogRead(TCL_POT3), 0, 1023, 0, 255);

    int colorSUM = (RED + BLUE + GREEN);
    ratioRED = ( (float)RED / colorSUM );
    ratioBLUE = ( (float)BLUE / colorSUM );
    ratioGREEN = ( (float)GREEN / colorSUM );

    ratioHIGHEST = ratioRED;
    if ( ratioHIGHEST < ratioBLUE ) {
      ratioHIGHEST = ratioBLUE;
    }
    if ( ratioHIGHEST < ratioGREEN ) {
      ratioHIGHEST = ratioGREEN;
    }
}


void color_picker() {
  int i; // A variable for looping
  static long nextupdate=0l; // Time when next update of colors should occur.
  long time = millis(); // Current time in milliseconds

  /* Move colors down the line by one */
  for(i=ACTIVELEDS-1;i>0;i--) {
    strand[i][0]=strand[i-1][0];
    strand[i][1]=strand[i-1][1];
    strand[i][2]=strand[i-1][2];
  }
  /* Read the current red value from potentiometer 1
   * Values are 10 bit and must be left shifted by 2 in order to fit in 8
   * bits */
  strand[0][0]=analogRead(TCL_POT1)>>2;

  /* Read the current green value from potentiometer 2 */
  strand[0][1]=analogRead(TCL_POT2)>>2;

  /* Read the current blue value from potentiometer 3 */
  strand[0][2]=analogRead(TCL_POT3)>>2;

  update_strand(); // Send all the pixels out
  delay( (int)map(analogRead(TCL_POT4), 0, 1023, 150, 0) );

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

  *r = rain_gamma_table[(byte)floor(r_f*255.99)];
  *g = rain_gamma_table[(byte)floor(g_f*255.99)];
  *b = rain_gamma_table[(byte)floor(b_f*255.99)];
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
    float flash_prob;

    if ( 1 == DevSheildInstalled ) {
      speed_pot = analogRead(TCL_POT1);
      brightness_pot = analogRead(TCL_POT2);
      saturation_pot = analogRead(TCL_POT3);
      flash_pot = analogRead(TCL_POT4);
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
        strand[i][0]=255;
        strand[i][1]=255;
        strand[i][2]=255;
      }
      else {
        rain_HSVtoRGB(local_h,sat,v,&strand[i][0],&strand[i][1],&strand[i][2]);
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


void DevBoardDetect() {
  int i;
  int DevSheldTestCount = 0;
  int DevSheldTest;
  DevSheldTest = analogRead(TCL_POT1);
  for(i=0; i<10; i++) {
    if ( (analogRead(TCL_POT1) < (DevSheldTest - 1) ) || (analogRead(TCL_POT1) > (DevSheldTest + 1) )    ) {
      DevSheldTestCount++;
    }
  }
  if ( 5 >= DevSheldTestCount ) {
    DevSheildInstalled = 1;
  }

}

void blackout_strand() {
  for(int i=0;i<MAXLEDS;i++) {
    strand[i][0]=0;
    strand[i][1]=0;
    strand[i][2]=0;
  }
  update_strand();
}

void whiteout_strand() {
  for(int i=0;i<MAXLEDS;i++) {
    strand[i][0]=255;
    strand[i][1]=255;
    strand[i][2]=255;
  }
  update_strand();
}



int readSettingsFromEEPROM(int defaultLEDs) {

  SettingsObject tempVar; //Variable to store custom object read from EEPROM.
  EEPROM.get(0, tempVar);

  if ( ((tempVar.NumberOfLEDs + 69) * 42) == tempVar.checksum) {
    return(tempVar.NumberOfLEDs);
  }
  else {
    return(defaultLEDs);
  }
}

void writeSettingsToEEPROM(int currentLEDs) {
  //Data to store.
  SettingsObject tempVar = {
    currentLEDs,
    (( currentLEDs + 69) * 42)
  };
  EEPROM.put(0, tempVar);
}
