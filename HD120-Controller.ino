
/* Yay Circles of Dooooooom! Gah. Anyway... after remapping, things should address kind of like:
 *          0  11  
 *        1      10
 *      2          9
 *      3          8
 *        4      7
 *          5  6
 *  
 */

#define FASTLED_INTERNAL
#include "FastLED.h"                // The actual LED control code. Library added aseparately.
#include <EEPROM.h>                 // We do not use EEPROM capability (yet) but hopefully will in the future.

FASTLED_USING_NAMESPACE
#define DATA_PIN            3       // Physical digital pin on the Arduino
#define LED_TYPE            WS2812B // DO NOT CHANGE for HD120 RGB
#define COLOR_ORDER         GRB     // DO NOT CHANGE for HD120 RGB unless hardware changes occur
#define FRAMES_PER_SECOND   60      // Coding out to 72 LEDs takes just about 3ms, so this works without interrupt issues.
#define BRIGHTNESS          128     // This is a safe brightness level for one fan powered wholly by USB.
#define NumberOfFans        6       // We will limit this to absolutely no more than one hub per controller because wiring two hubs together is a pain
#define LedsPerFan          12      // 12 for HD120 RGB.
#define NUM_LEDS            (NumberOfFans * LedsPerFan)

/* SubSetsPerFan is not actually configurable but explained here anyway.
// Defining the SubSets is hard-coded so cannot translate between fans or different subsets programmatically
//------------------------------------------------
// 0 = Full Fan counter-clockwise
// 1 = NW CCW
// 2 = SW CCW
// 3 = SE CCW
// 4 = NE CCW
// 5 = West Side Bottom to Top
// 6 = East Side Bottom to Top */
#define SubSetsPerFan        7

/*---- SettingGroups ---------------------------------------------
// 0 = Global
// 1 to NumberOfFans = Per Fan 
// I could have made 0 though x the fans, but then Global would always be at a different place. Trade either way, so in Fan settings I just x + 1 it */
#define SettingGroups        (1 + NumberOfFans)

//---- SettingItems ----------------------------------------------
// Global  ================\
// 0   = Brightness
// 1   = Global frame rotation
// 2-7 = Future Use
// Per Fan ================\
// 0   = Mode
// 1-7 = Mode-dependant data
#define SettingItems         8

// The actual LED Arrays which are built as Sets with Arrays
CRGBArray<NUM_LEDS> leds;    // Operate on this array as if it had the correct layout.
CRGBArray<NUM_LEDS> actual;  // Display this array after mapping "leds" to "actual". In most code, ignore this.

CRGBSet *fan[NumberOfFans][SubSetsPerFan];  //Fan Subset Array
uint8_t gSettings[SettingGroups][SettingItems]; //Settings Array

#define NumberOfModes       4 //Raise this as modes are created. Used to sanity-check settings.
//===========================================================================================================
// Mode Functions
//===========================================================================================================
//In order for the pointers to work, all modes need to be defined first. 

//--- mode0() -----------------------------------------------------------------------------------------------
// Angry Myia Mode 
// A simple 10 second pulse between teal and red, crossing through blue and purple.
// Settings: None
void mode0(uint8_t thisFan) { 
  fan[thisFan][0]->operator()(0,LedsPerFan-1) = CHSV(beatsin8(6, 128, 255),255,255);
}

//--- mode1() -----------------------------------------------------------------------------------------------
// Single Color Fan
// Settings: 1 = Hue
void mode1(uint8_t thisFan) {
  fan[thisFan][0]->operator()(0,LedsPerFan-1) = CHSV(gSettings[thisFan+1][1],255,255);
}

//--- mode2() -----------------------------------------------------------------------------------------------
// Single color sine wave pulse
// Settings: 1 = Hue, 2 = BPM (60 / BPM = Time for fade cycle))
void mode2(uint8_t thisFan) {
  gSettings[thisFan+1][2] = (gSettings[thisFan+1][2] < 1) ? 6 : gSettings[thisFan+1][2]; // 0 BPM is x/0 and is bad, mmkay? But we will default to a decent speed.
  fan[thisFan][0]->operator()(0,LedsPerFan-1) = CHSV(gSettings[thisFan+1][1],255,beatsin8(gSettings[thisFan+1][2],0,255));
}

//--- mode3() -----------------------------------------------------------------------------------------------
// Rotating full rainbow
// Settings: None
void mode3(uint8_t thisFan) {
  fan[thisFan][0]->operator()(0,LedsPerFan-1).fill_rainbow((gSettings[0][1]),19);
}

//===========================================================================================================
//=--- End of Mode Functions ---=============================================================================
//===========================================================================================================

// Mode function pointer array to translate things cleanly from incoming data to mode.
typedef void (*modefunarr)(uint8_t);
static modefunarr modefun[NumberOfModes] = {
  mode0,
  mode1,
  mode2,
  mode3
};

// Serial Handling
int inByte = 0;



//==== Arduino setup Function ===============
void setup() {
  delay(3000); // 3 second delay for recovery -> This is IMPORTANT
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(actual, NUM_LEDS); //.setCorrection(TypicalLEDStrip); // The color correction makes things "Video correct" but we want pretty, not video.
  /* Keep this for legacy reference until defineSets() is confirmed working
  fan[0][0] = new CRGBSet( leds(0, 11) );
  fan[0][1] = new CRGBSet( leds(6, 11) );*/
  defineSets();
  initSettings();
  Serial.begin(115200);
}

//==== Arduino Main Loop ====================
void loop() {
  //fan[0][0]->operator()(0,2) = CRGB::Red; //Poke several LEDs in one fan
  //leds[4] = CRGB::Green; //Address the full strip directly
  //fan[0][1]->operator()(0,2) = CRGB::Green; //Poke several LEDs in one fan
  //(*fan[0][1])[3] = CRGB::Gray; //Address one specific LED in one fan
  breakfast();
  processFans();
 
  remap();  //***Always run the remap function just before calling show().***
  FastLED.setBrightness(gSettings[0][0]);
  FastLED.show();
  FastLED.delay((1000/FRAMES_PER_SECOND) - 2); //Approximately 2 ms goes into writing to the LEDs (27 us per LED)

}





//---- defineSets() ----------------------------------
// Set up Fans and other sets
void defineSets() {
  //Since the subsets are pretty much hard-coded, some will need to be defined by hand.
  //------------------------------------------------
  // 0 = Full Fan counter-clockwise
  // 1 = NW CCW
  // 2 = SW CCW
  // 3 = SE CCW
  // 4 = NE CCW
  // 5 = West Side Bottom to Top
  // 6 = East Side Bottom to Top
  //------------------------------------------------
  // Set up full fans
  for (uint8_t i = 0; i < NumberOfFans; i++) {
    fan[i][0] = new CRGBSet( leds((i * LedsPerFan), (((i + 1) * LedsPerFan) - 1)));
  }
  // Set up quadrants
  for (uint8_t i = 0; i < NumberOfFans; i++) {
    for (uint8_t q = 1; q < 5; q++) {
      fan[i][q] = new CRGBSet( leds((i * LedsPerFan) + (q * 3), ((i * LedsPerFan) + ((q + 1) * 3) - 1)));
    }
  }
  // Set up sides
  for (uint8_t i = 0; i < NumberOfFans; i++) {
    fan[i][5] = new CRGBSet( leds((i * LedsPerFan) + 5, (i * LedsPerFan)));
    if (!(fan[i][6] = new CRGBSet( leds((i * LedsPerFan) + 6, (((i + 1) * LedsPerFan) - 1))) ) ) { 
      actual(0,3) = CRGB::Red; // This happens if we ran out of memory.
      FastLED.show();
      exit(1);
    }
  }
}

//---- initSettings() --------------------------------
// For now just a basic set of things. In the future, 
// possibly check EEPROM. This goes into gSettings.
void initSettings() {
  for (int i = 0; i < SettingGroups; i++) {
    for (int q = 0; q < SettingItems; q++) {
      gSettings[i][q] = 0;
    }
  }
  gSettings[0][0] = BRIGHTNESS; //Set the initial brightness
}


//---- remap()----------------------------------------
// Function to remap "leds" array to "actual" array - 
// Cleaned up and shrunk
void remap() {
  for (uint8_t i=0; i < NUM_LEDS; i++) {
    if ( (i % LedsPerFan == 0) || (i % LedsPerFan == 1) ) {
      actual[i] = leds[(floor(i / LedsPerFan) * LedsPerFan) + 10 + (i % 12)];
    } else {
      actual[i] = leds[i-2];
    }
  }
}

//---- processFans() ---------------------------------
// Primary container to step through the fans and 
// call the mode function for each one
void processFans() {
  gSettings[0][1]++; //Roll the frame counter up
  for (uint8_t i = 0; i < NumberOfFans; i++) { // Step through each fan
    if (gSettings[i+1][0] >= NumberOfModes) { gSettings[i+1][0] = 0; } // Sanity check.
    modefun[gSettings[i+1][0]](i);
  }
}

//---- breakfast() -----------------------------------
// Handle the serial data. Currently Human-readable
void breakfast() {
  if (Serial.available() < 1) { return; } // We have nothing to do
  //Got to here? We have serial. Let's Do A Thing With It. 
  if (!(Serial.read() == 62)) {
    // Not a valid header
    serialDump(); //Clear everything else - Yes, constant serial flood will halt all lighting
    Serial.println("No");
    return;
  }
  Serial.println("Go");
  int rg = Serial.parseInt();
  int ri = Serial.parseInt();
  int rv = Serial.parseInt();
  serialDump();
  uint8_t ig = constrain(rg,0,(SettingGroups - 1));
  uint8_t ii = constrain(ri,0,(SettingItems - 1));
  uint8_t iv = constrain(rv,0,255);
  Serial.print("OK ");
  Serial.print(ig, DEC);
  Serial.print(" ");
  Serial.print(ii, DEC);
  Serial.print(" ");
  Serial.println(iv, DEC);
  gSettings[ig][ii] = iv;
}

//---- serialDump() -----------------------------------
// We don't like the serial so we dump it all out
void serialDump() {
  while (Serial.available()) {
    byte trash = Serial.read();
  }
}

