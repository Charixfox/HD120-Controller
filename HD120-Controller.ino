/* Yay Circles of Dooooooom! Gah. Anyway... after remapping, things should address kind of like:
            0  11
          1      10
        2          9
        3          8
          4      7
            5  6

    Version score 0.2.1.3 - RC FI

    TODO...
    - Multi-Fan coordination modes. For example a chaser that traverses fans and is side-aware.
    - Develop better control protocol and communication system and possibly a PC program. ... Yeah, right. Halp? Anyone?

    Development Notes:
    Arduino AVRs couldn't give two giggles about divide by zero. It will not cause an error. It returns -1 or 0 depending on signed or unsigned.
*/

#define FASTLED_INTERNAL
#include "FastLED.h"                // The actual LED control code. Library added aseparately.
#include <avr/eeprom.h>             // Default EEPROM library from the IDE

FASTLED_USING_NAMESPACE
// Basic Configuration
#define DATA_PIN            3       // Physical digital pin on the Arduino
#define STRIP_PIN           4       // Physical digital pin for strips



/*Only change things below this if you understand and know what you are doing.
// Having too many things is fine and doesn't hurt anything, so even if you only
// have four fans or two or no strips, you can leave this all be. */
#define LED_TYPE            WS2812B // DO NOT CHANGE for HD120 RGB
#define COLOR_ORDER         GRB     // DO NOT CHANGE for HD120 RGB unless hardware changes occur
#define BRIGHTNESS          128     // This is a safe brightness level for one fan powered wholly by USB.

#define SETCOMPAT           1       // Defines the current compatibility level of the EEPROM storage.

// Fan definitions
#define NumberOfFans        6       // We will limit this to absolutely no more than one hub per controller because wiring two hubs together is a pain
#define LedsPerFan          12      // 12 for HD120 RGB.
#define NUM_LEDS            (NumberOfFans * LedsPerFan)


/* Additional support for LED Strips on Pin 4
// - Technically we can support more strips, however I recommend against it in the stock setup. Please keep power draw (90mA/LED max) and connector/cable limits in mind
// - If you don't know what the above means, don't use more than four strips. <.< */
#define NumberOfStrips      4
#define LedsPerStrip        10
#define STRIP_LEDS          (NumberOfStrips * LedsPerStrip)

// Um, what?
#if NumberOfStrips == 0 && NumberOfFans == 0
  #error "You need to have at least one strip or fan."
#endif


// EVIL macros. Evil, Evil, Evil
#if NumberOfStrips
  #define EM_S                (*strip[thisStrip])  // Addressing for strip modes that use thisStrip to grab the whole strip
#endif
#if NumberOfFans
#define EM_F                (*fan[thisFan][0])
// Evil Macros for Fan Sides
#define EM_FW               (*fan[thisFan][5])
#define EM_FE               (*fan[thisFan][6])
// Evil Macros for Fan Quarters
#define EM_FNW              (*fan[thisFan][1])
#define EM_FNE              (*fan[thisFan][4])
#define EM_FSW              (*fan[thisFan][2])
#define EM_FSE              (*fan[thisFan][3])
#endif

/* Group Handling
 *  A group requires at least two fans. We will currently support grouping fans only. Strips are sad anyway.
 *  Since the smallest group is two fans, and any bigger group of fans will mean fewer groups, we will set
 *  the MaxGroups as Fans/2 -- Arduino should truncate the halves?
 */
#define MaxGroups           (NumberOfFans / 2)

/* SubSetsPerFan is not actually configurable but explained here anyway.
  // Defining the SubSets is hard-coded maths so cannot translate between fans or different subsets programmatically
  //------------------------------------------------
  // 0 = Full Fan counter-clockwise
  // 1 = NW CCW
  // 2 = SW CCW
  // 3 = SE CCW
  // 4 = NE CCW
  // 5 = West Side Bottom to Top
  // 6 = East Side Bottom to Top */
#if NumberOfFans
#define SubSetsPerFan        7
#endif

/*---- SettingGroups ---------------------------------------------
 * 0 = Global
 * 1 to NumberOfFans = Per Fan
 * NumberOfFans + 1 to NumberOfFans + NumberOfStrips = Per strip (For example, 6 fans and 4 strips, 7-10 = strips
 * Last Strip + 1 to + MaxGroups = Per Grouping
 * I could have made 0 though x the fans, but then Global would always be at a different place. Trade either way, so in Fan settings I just x + 1 it
 */
#define SettingGroups        (1 + NumberOfFans + NumberOfStrips + MaxGroups)

/*---- SettingItems ----------------------------------------------
// -- Global  ================\
// 0   = Remembered Global Brightness
// 1   = Global Mode
// 2-5 = Global Mode Data
// 6   = Sync BPM
// 7   = BPM
// -- Per Fan or Strip =======\
// 0   = Mode
// 1-7 = Mode-dependant data
// ---> Notes
// - Regardless of the number of setting items, the initial setup will set the last two to be 6.
//    This is useful for BPM rates
*/
#define SettingItems        8

// The actual LED Arrays which are built as Sets with Arrays
#if NumberOfFans
CRGBArray<NUM_LEDS>   leds;    // Operate on this array as if it had the correct layout.
CRGBArray<NUM_LEDS>   actual;  // Display this array after mapping "leds" to "actual". In most code, ignore this.
/* Note on Scratch Space:
 *  The "actual" array can also be used for scratch functions after display is called and before remap is called. This
 *  is the majority of the code. However: Mapped segments from leds must be de-mapped back to the scratch space prior
 *  to operating on scratch space in the event the function modifies prior existing data (like fading movement).
 */
#endif
#if NumberOfStrips
CRGBArray<STRIP_LEDS> stripLeds;  // Strips in order
#endif

#if NumberOfFans
CRGBSet *fan[NumberOfFans][SubSetsPerFan];      // Fan Subset Array
#endif
#if NumberOfStrips
CRGBSet *strip[NumberOfStrips];                 // Strips Array
#endif
uint8_t gSettings[SettingGroups][SettingItems]; // Settings Array
uint8_t sSettings[SettingGroups][SettingItems]; // Stage Settings Array
uint8_t *gMap[MaxGroups][2];                    // Pointer array for grouping mappers


#if NumberOfFans
#define NumberOfModes       11 // Raise this as modes are created. Used to sanity-check settings.
#endif
#if NumberOfStrips
#define NumberOfStripModes  8 // Raise this as modes are created. Used to sanity-check settings.
#endif
#define NumberOfGlobalModes 3 // Raise this as modes are created. Used to sanity-check settings.
#define NumberOfGroupModes  1 // Raise this as modes are created. Used to sanity-check settings.
//===========================================================================================================
// Mode Functions
/*===========================================================================================================
//In order for the pointers to work, all modes need to be defined first.

//-- Working with Fan LEDs - Dev notes
// fan[0][0]->operator()(0,2) = CRGB::Red;  // Poke several LEDs in one fan. This CAN be used but is substantially-larger machine code.
// (*fan[0][1])(2,3) = CRGB::Gray;          // Poke several LEDs in one fan. Smaller Code.
// leds[4] = CRGB::Green;                   // Address the full strip directly. But why?
// (*fan[0][1])[3] = CRGB::Gray;            // Address one specific LED in one fan
// (*fan[FanNumber][Segment]) then things   // Best overall.
*/

#if NumberOfFans


//-- mode0() -----------------------------------------------------------------------------------------------
// Hue Shift
// Settings: 1 = Starting Hue; 2 = Ending Hue; 3 = Hue Offset; 5 = Phase Offset; 7 = Rate in BPM
void mode0(uint8_t thisFan) {
  EM_F = CHSV(beatsin8(rFS(thisFan, 7), rFS(thisFan, 1), rFS(thisFan, 2), 0, rFS(thisFan, 5)) + rFS(thisFan, 3), 255, 255);
}

//-- mode1() -----------------------------------------------------------------------------------------------
/* Single Spinner
// Settings: 1 = Hue (Overridden by 3); 2 = 0 -> Clockwise, 1+ -> Counterclockwise;
//           3 = Rainbowmode? 0 -> No, 1+ -> Rainbow; 4 = BPM of Rainbow shift;
//           5 = Blade Offset; 6 = Fade Speed (good to match BPM); 7 = Spin Speed BPM */
void mode1(uint8_t thisFan) {
  uint8_t theHue;
  uint8_t bpm = rFS(thisFan, 7);
  uint8_t fbo = rFS(thisFan,5);

  switch (rFS(thisFan, 3)) {
    case 0 :
      theHue = rFS(thisFan, 1);
      break;
    default :
      theHue = beat8(rFS(thisFan, 4));
  }
  EM_F.fadeToBlackBy(rFS(thisFan, 6));
  switch (rFS(thisFan, 2)) {
    case 0 :
      (*fan[thisFan][0])[(LedsPerFan - 1 - scale8(beat8(bpm), LedsPerFan - 1) + fbo) % LedsPerFan] = CHSV(theHue, 255, 255);
      break;
    default :
      (*fan[thisFan][0])[(scale8(beat8(bpm), LedsPerFan - 1) + fbo) % LedsPerFan] = CHSV(theHue, 255, 255);
  }
}

//-- mode2() -----------------------------------------------------------------------------------------------
/* Rainbow
// Settings: 1 = Chance of Sparkles (0-255); 2 = Hue Steps per LED -- 21 shows a full rainbow; 
//           3 = Fan Hue Offset; 4 = 0 -> Normal Rotation, 1+ -> Reverse Rotation; 7 = Speed of Rotation  */
void mode2(uint8_t thisFan) {
  switch (rFS(thisFan, 4)) {
    case 0 :
      EM_F.fill_rainbow(beat8(rFS(thisFan, 7)) + rFS(thisFan, 3), rFS(thisFan, 2)); 
      break;
    default :
      EM_F.fill_rainbow(255 - (beat8(rFS(thisFan, 7)) + rFS(thisFan, 3)), rFS(thisFan, 2)); 
  }
  if (random8() < rFS(thisFan, 1)) {
    (*fan[thisFan][0])[random8(LedsPerFan)] += CRGB::White;
  }
}

//-- mode3() -----------------------------------------------------------------------------------------------
/* Four-point spinner
// Settings: 1 = Hue (Overridden by 3); 2 = 0 -> Clockwise, 1+ -> Counterclockwise;
//           3 = Rainbowmode? 0 -> No, 1+ -> Rainbow;
//           4 = BPM of Rainbow shift; 5 = Per Blade Hue Shift; 6 = Fade Speed (good to match twice BPM);
//           7 = Spin Speed BPM  */
void mode3(uint8_t thisFan) {
  uint8_t theHue, rbm, shift;
  rbm = rFS(thisFan, 3);
  shift = rFS(thisFan, 5);
  uint8_t bpm = rFS(thisFan, 7);

  switch (rbm) {
    case 0 :
      theHue = rFS(thisFan, 1);
      break;
    default :
      theHue = beat8(rFS(thisFan, 4));
  }
  EM_F.fadeToBlackBy(rFS(thisFan, 6));
  if (rFS(thisFan, 2) > 0) {
    EM_F[scale8(beat8(bpm), LedsPerFan - 1) % LedsPerFan] = CHSV(theHue, 255, 255);
    EM_F[(scale8(beat8(bpm), LedsPerFan - 1) + (LedsPerFan / 4)) % LedsPerFan] = CHSV(theHue + shift, 255, 255);
    EM_F[(scale8(beat8(bpm), LedsPerFan - 1) + (2 * LedsPerFan / 4)) % LedsPerFan] = CHSV(theHue + (shift * 2), 255, 255);
    EM_F[(scale8(beat8(bpm), LedsPerFan - 1) + (3 * LedsPerFan / 4)) % LedsPerFan] = CHSV(theHue + (shift * 3), 255, 255);
  }
  else {
    EM_F[LedsPerFan - 1 - scale8(beat8(bpm), LedsPerFan - 1) % LedsPerFan] = CHSV(theHue, 255, 255);
    EM_F[LedsPerFan - 1 - (scale8(beat8(bpm), LedsPerFan - 1) + (LedsPerFan / 4)) % LedsPerFan] = CHSV(theHue + shift, 255, 255);
    EM_F[LedsPerFan - 1 - (scale8(beat8(bpm), LedsPerFan - 1) + (2 * LedsPerFan / 4)) % LedsPerFan] = CHSV(theHue + (shift * 2), 255, 255);
    EM_F[LedsPerFan - 1 - (scale8(beat8(bpm), LedsPerFan - 1) + (3 * LedsPerFan / 4)) % LedsPerFan] = CHSV(theHue + (shift * 3), 255, 255);
  }
}

//-- mode4() -----------------------------------------------------------------------------------------------
/* Double-scan
// Settings: 1 = Hue (Overridden by 3); 2 = Rotation Offset; 3 = Rainbowmode? 0 -> No, 1 -> Rainbow
//           4 = BPM of Rainbow shift; 5 = Per Blade Hue Shift; 6 = Fade Speed (good to match BPM);
//           7 = Spin Speed BPM  */
void mode4(uint8_t thisFan) {
  uint8_t theHue, rbm, shift;
  rbm = rFS(thisFan, 3);
  shift = rFS(thisFan, 5);
  uint8_t bpm = rFS(thisFan, 7);

  switch (rbm) {
    case 0 :
      theHue = rFS(thisFan, 1);
      break;
    default :
      theHue = beat8(rFS(thisFan, 4));
  }
  EM_F.fadeToBlackBy(rFS(thisFan, 6));
  EM_F[(scale8(beat8(bpm), LedsPerFan - 1) + rFS(thisFan, 2)) % LedsPerFan] = CHSV(theHue, 255, 255);
  EM_F[(LedsPerFan - 1 - scale8(beat8(bpm), LedsPerFan - 1) + rFS(thisFan, 2)) % LedsPerFan] = CHSV(theHue + shift, 255, 255);
}

//-- mode5() -----------------------------------------------------------------------------------------------
/* Double Spinner
// Settings: 1 = Hue (Overridden by 3); 2 = 0 -> Clockwise, 1+ -> Counterclockwise;
//           3 = Rainbowmode? 0 -> No, 1+ -> Rainbow;
//           4 = BPM of Rainbow shift; 5 = Per Blade Hue Shift; 6 = Fade Speed (good to match BPM);
//           7 = Spin Speed BPM */
void mode5(uint8_t thisFan) {
  uint8_t theHue, rbm, shift;
  rbm = rFS(thisFan, 3);
  shift = rFS(thisFan, 5);
  uint8_t bpm = rFS(thisFan, 7);

  switch (rbm) {
    case 0 :
      theHue = rFS(thisFan, 1);
      break;
    default :
      theHue = beat8(rFS(thisFan, 4));
  }
  EM_F.fadeToBlackBy(rFS(thisFan, 6));
  if (rFS(thisFan, 2) > 0) {
    EM_F[scale8(beat8(bpm), LedsPerFan - 1) % LedsPerFan] = CHSV(theHue, 255, 255);
    EM_F[(scale8(beat8(bpm), LedsPerFan - 1) + (LedsPerFan / 2)) % LedsPerFan] = CHSV(theHue, 255, 255);
  }
  else {
    EM_F[LedsPerFan - 1 - scale8(beat8(bpm), LedsPerFan - 1) % LedsPerFan] = CHSV(theHue, 255, 255);
    EM_F[LedsPerFan - 1 - (scale8(beat8(bpm), LedsPerFan - 1) + (LedsPerFan / 2)) % LedsPerFan] = CHSV(theHue, 255, 255);
  }
}

//-- mode6() -----------------------------------------------------------------------------------------------
/*  BPM from 100-line demo
// Settings: 7 = BPM; 1 = Hue multiplier; 2 = Beat multiplier */
void mode6(uint8_t thisFan) {
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8( rFS(thisFan, 7), 64, 255);
  uint8_t counter = beat8(6);
  for ( int i = 0; i < LedsPerFan; i++) {
    EM_F[i] = ColorFromPalette(palette, counter + (i * (rFS(thisFan, 1))), beat - counter + (i * rFS(thisFan, 2)));
  }
}

//-- mode7() -----------------------------------------------------------------------------------------------
/* Split Sides
// Settings: 7 = BPM of pulse; 1 = W Side hue; 2 = E Side hue; 4 = Fan Phase Offset; 5 = Per side phase offset;
//           6 = Pulse? 0 -> No, 1 -> Sin, 2 -> Sawtooth In, 3 -> Sawtooth Out, 4+ -> Triangle */
void mode7(uint8_t thisFan) {
  uint8_t beat;
  uint8_t bpm = rFS(thisFan, 7);
  uint8_t fpo = rFS(thisFan, 4);
  uint8_t pso = rFS(thisFan, 5);
  uint8_t huea = rFS(thisFan,1);
  uint8_t hueb = rFS(thisFan,2);
  switch (rFS(thisFan, 6)) {
    case 0 :
      EM_FE = CHSV(hueb, 255, 255);
      EM_FW = CHSV(huea, 255, 255);
      return;
    case 1 :
      EM_FW = CHSV(huea, 255, beatsin8(bpm, 0, 255, 0, fpo));
      EM_FE = CHSV(hueb, 255, beatsin8(bpm, 0, 255, 0, pso + fpo));
      return;
    case 2 :
      beat = beat8(bpm) + fpo;
      break;
    case 3 :
      beat = 255 - beat8(bpm) + fpo;
      break;
    default :
      beat = triwave8(beat8(bpm)) + fpo;
  }
  EM_FW = CHSV(huea, 255, beat);
  EM_FE = CHSV(hueb, 255, beat + pso);
}

/* Code notes:
// On West side, the iterators and fill functions do not honor the reverse (bottom to top) order
// (*fan[thisFan][5]).fill_rainbow(0,42) -> This will fill top to bottom with a rainbow.
// (*fan[thisFan][5]) =  -> This will address the whole segment properly.
*/


//-- mode8() -----------------------------------------------------------------------------------------------
/* Split Quarters
// Settings: 1 = NW Side hue; 2 = NE Side hue; 3 = SE Side hue; 4 = SW Side hue;
//           5 = Per side phase offset;
//           6 = Pulse? 0 -> No, 1 -> Sin, 2 -> Sawtooth In, 3 -> Sawtooth Out, 4+ -> Triangle
//           7 = BPM of pulse;  */
void mode8(uint8_t thisFan) {
  uint8_t beat;
  uint8_t bpm = rFS(thisFan, 7);
  uint8_t pso = rFS(thisFan, 5);
  uint8_t huea = rFS(thisFan,1);
  uint8_t hueb = rFS(thisFan,2);
  uint8_t huec = rFS(thisFan,3);
  uint8_t hued = rFS(thisFan,4);
  switch (rFS(thisFan, 6)) {
    case 0 :
      EM_FNW = CHSV(huea, 255, 255);
      EM_FNE = CHSV(hueb, 255, 255);
      EM_FSE = CHSV(huec, 255, 255);
      EM_FSW = CHSV(hued, 255, 255);
      return;
    case 1 :
      EM_FNW = CHSV(huea, 255, beatsin8(bpm));
      EM_FNE = CHSV(hueb, 255, beatsin8(bpm, 0, 255, 0, pso));
      EM_FSE = CHSV(huec, 255, beatsin8(bpm, 0, 255, 0, pso * 2));
      EM_FSW = CHSV(hued, 255, beatsin8(bpm, 0, 255, 0, pso * 3));
      return;
          case 2 :
      beat = beat8(bpm);
      break;
    case 3 :
      beat = 255 - beat8(bpm);
      break;
    default :
      beat = triwave8(beat8(bpm));
  }
      EM_FNW = CHSV(huea, 255, beat);
      EM_FNE = CHSV(hueb, 255, beat + pso);
      EM_FSE = CHSV(huec, 255, beat + pso * 2);
      EM_FSW = CHSV(hued, 255, beat + pso * 3);
}

//-- mode9() -----------------------------------------------------------------------------------------------
// Set RGB Color
// 1 = R; 2 = G; 3 = B
void mode9(uint8_t thisFan) {
  EM_F = CRGB(rFS(thisFan,1),rFS(thisFan,2),rFS(thisFan,3));
}

//-- mode10() -----------------------------------------------------------------------------------------------
// Fade to Black (No further color changes)
// Settings:  1 = Fade Rate
void mode10(uint8_t thisFan) {
    EM_F.fadeToBlackBy(rFS(thisFan, 1));
}
#endif


//===========================================================================================================
//=--- Strip Mode Functions ---==============================================================================
//===========================================================================================================
#if NumberOfStrips

//-- smode0() -----------------------------------------------------------------------------------------------
/* Hue Shift
// Settings: 1 = Starting Hue; 2 = Ending Hue; 3 = Hue Offset; 5 = Phase Offset; 7 = Rate in BPM */
void smode0(uint8_t thisStrip) {
  EM_S = CHSV(beatsin8(rSS(thisStrip, 7), rSS(thisStrip, 1), rSS(thisStrip, 2), 0, rSS(thisStrip, 5)) + rSS(thisStrip, 3), 255, 255);

}

//-- smode1() -----------------------------------------------------------------------------------------------
/* Single Color Strip
// Settings: 1 = Hue */
void smode1(uint8_t thisStrip) {
  EM_S = CHSV(rSS(thisStrip, 1), 255, 255);
}

//-- smode2() -----------------------------------------------------------------------------------------------
/* Single color pulse
// Settings: 1 = Hue, 7 = BPM (60 / BPM = Time for fade cycle)) */
void smode2(uint8_t thisStrip) {
  EM_S = CHSV(rSS(thisStrip, 1), 255, beatsin8(rSS(thisStrip, 7), 0, 255));
}

//-- smode3() -----------------------------------------------------------------------------------------------
/* Running full rainbow
// Settings: 7 = Speed in BPM */
void smode3(uint8_t thisStrip) {
  EM_S.fill_rainbow(beat8(rSS(thisStrip, 7)), 25); // 25 is the steps between hues between LEDs which aligns the rainbow
}

//-- smode4() -----------------------------------------------------------------------------------------------
/* Prototype Testing - Currently BPM from 100-line demo
// Settings: 7 = BPM; 1 = Hue multiplier; 2 = Beat multiplier */
void smode4(uint8_t thisStrip) {
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8( rSS(thisStrip, 7), 64, 255);
  uint8_t counter = beat8(6);
  for ( int i = 0; i < LedsPerStrip; i++) {
    (*strip[thisStrip])[i] = ColorFromPalette(palette, counter + (i * rSS(thisStrip, 1)), beat - counter + (i * rSS(thisStrip, 2)));
  }
}

//-- smode5() -----------------------------------------------------------------------------------------------
/* Set RGB Color
// 1 = R; 2 = G; 3 = B */
void smode5(uint8_t thisStrip) {
  EM_S = CRGB(rSS(thisStrip,1),rSS(thisStrip,2),rSS(thisStrip,3));
}

//-- smode6() -----------------------------------------------------------------------------------------------
/* Double-scan
// Settings: 1 = Hue (Overridden by 3); 2 = Rotation Offset; 3 = Rainbowmode? 0 -> No, 1 -> Rainbow
//           4 = BPM of Rainbow shift; 5 = Per Blade Hue Shift; 6 = Fade Speed (good to match BPM);
//           7 = Spin Speed BPM  */
void smode6(uint8_t thisStrip) {
  uint8_t theHue, rbm, shift;
  rbm = rSS(thisStrip, 3);
  shift = rSS(thisStrip, 5);
  uint8_t bpm = rSS(thisStrip, 7);

  switch (rbm) {
    case 0 :
      theHue = rSS(thisStrip, 1);
      break;
    default :
      theHue = beat8(rSS(thisStrip, 4));
  }
  EM_S.fadeToBlackBy(rSS(thisStrip, 6));
  EM_S[(scale8(beat8(bpm), LedsPerStrip - 1) + rSS(thisStrip, 2)) % LedsPerStrip] = CHSV(theHue, 255, 255);
  EM_S[(LedsPerStrip - 1 - scale8(beat8(bpm), LedsPerStrip - 1) + rSS(thisStrip, 2)) % LedsPerStrip] = CHSV(theHue + shift, 255, 255);
}

//-- smode7() -----------------------------------------------------------------------------------------------
/* Double Runner
// Settings: 1 = Hue (Overridden by 3); 2 = 0 -> Clockwise, 1+ -> Counterclockwise;
//           3 = Rainbowmode? 0 -> No, 1+ -> Rainbow;
//           4 = BPM of Rainbow shift; 5 = Per Blade Hue Shift; 6 = Fade Speed (good to match BPM);
//           7 = Spin Speed BPM */
void smode7(uint8_t thisStrip) {
  uint8_t theHue, rbm, shift;
  rbm = rSS(thisStrip, 3);
  shift = rSS(thisStrip, 5);
  uint8_t bpm = rSS(thisStrip, 7);

  switch (rbm) {
    case 0 :
      theHue = rSS(thisStrip, 1);
      break;
    default :
      theHue = beat8(rSS(thisStrip, 4));
  }
  EM_S.fadeToBlackBy(rSS(thisStrip, 6));
  if (rSS(thisStrip, 2) > 0) {
    EM_S[scale8(beat8(bpm), LedsPerStrip - 1) % LedsPerStrip] = CHSV(theHue, 255, 255);
    EM_S[(scale8(beat8(bpm), LedsPerStrip - 1) + (LedsPerStrip / 2)) % LedsPerStrip] = CHSV(theHue, 255, 255);
  }
  else {
    EM_S[LedsPerStrip - 1 - scale8(beat8(bpm), LedsPerStrip - 1) % LedsPerStrip] = CHSV(theHue, 255, 255);
    EM_S[LedsPerStrip - 1 - (scale8(beat8(bpm), LedsPerStrip - 1) + (LedsPerStrip / 2)) % LedsPerStrip] = CHSV(theHue, 255, 255);
  }
}





#endif


//===========================================================================================================
//=--- Global Mode Functions ---=============================================================================
//===========================================================================================================

//-- gmode0() -----------------------------------------------------------------------------------------------
// Absolutely nothing. No global override.
void gmode0() {
  return;
}

//-- gmode1() -----------------------------------------------------------------------------------------------
/* Pulse brightness between 0 and max brightness setting
// Settings: 7 = Speed in BPM */
void gmode1() {
  FastLED.setBrightness(beatsin8(rGS(7), 0, rGS(0)));
}

//-- gmode2() -----------------------------------------------------------------------------------------------
/* Flash one fan - Special Effect Trigger, ID fan
// Resets to global mode 0 afterward
// Settings: 2 = Fan to flash; 3 = Red Channel; 4 = Green Channel; 5 = Blue Channel */
void gmode2() {
  uint8_t thisFan = rGS(2)-1;
  #if NumberOfFans
  EM_F = CRGB(rGS(3), rGS(4), rGS(5));
  #endif
  wGS(1, 0);
  sSettings[0][1] = 0; // Also wipe stage settings to avoid repeat during sync
}



//===========================================================================================================
//=--- End of Mode Functions ---=============================================================================
//===========================================================================================================


// Mode function pointer array to translate things cleanly from incoming data to mode.
// Define the "modefunarr" type array of pointers to functions that take one thing.
typedef void (*modefunarr)(uint8_t);

#if NumberOfFans
static modefunarr modefun[NumberOfModes] = {
  mode0,
  mode1,
  mode2,
  mode3,
  mode4,
  mode5,
  mode6,
  mode7,
  mode8,
  mode9,
  mode10
};
#endif

#if NumberOfStrips
static modefunarr smodefun[NumberOfStripModes] = {
  smode0,
  smode1,
  smode2,
  smode3,
  smode4,
  smode5,
  smode6,
  smode7
};
#endif

/*static modefunarr grmodefun[NumberOfGroupModes] = {
  grmode0
};*/

typedef void (*gmodefunarr)(); // typedef without parameters for global modes
static gmodefunarr gmodefun[NumberOfGlobalModes] = {
  gmode0,
  gmode1,
  gmode2
};

// Serial Handling
int inByte = 0;

// Save handling
uint8_t EEMEM eeHeader[3], eeSettings[(SettingGroups * SettingItems)]; // Stores the data in EEPROM when saved
uint8_t eeCompat[3]; // For checking the EEPROM header

//==== Arduino setup Function ===============
void setup() {
  delay(3000); // 3 second delay for recovery -> This is IMPORTANT
  #if NumberOfFans
    FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(actual, NUM_LEDS);
  #endif
  #if NumberOfStrips
    FastLED.addLeds<LED_TYPE, STRIP_PIN, COLOR_ORDER>(stripLeds, STRIP_LEDS);
  #endif
  defineSets();
  initSettings();
  Serial.begin(115200);
  checkStorage();
}


//==== Arduino Main Loop ====================
void loop() {
  FastLED.setBrightness(rGS(0)); // We'll do this first since Global may override it.
  breakfast();
  #if NumberOfFans
    processFans();
  #endif
  #if NumberOfStrips
    processStrips();
  #endif
  processGlobal();
  #if NumberOfFans
    remap();                       //***Always run the remap function just before calling show().***
  #endif
  FastLED.delay(13); // This calls show as many times before the next FPS-based recalc
}





//---- defineSets() --------------------------------
// Set up Fans and other sets
void defineSets() {
#if NumberOfStrips
// Set up full strips
  for (uint8_t i = 0; i < NumberOfStrips; i++) {
    strip[i] = new CRGBSet( stripLeds((i * LedsPerStrip), (((i + 1) * LedsPerStrip) - 1)));
  }
#endif
#if NumberOfFans
//Since the subsets are pretty much hard-coded, some will need to be defined by hand.
  //This will NOT work if fans with more than 12 LEDs come out. T^T
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
      fan[i][q] = new CRGBSet( leds((i * LedsPerFan) + ((q - 1) * 3), ((i * LedsPerFan) + (q * 3) - 1)));
    }
  }
  // Set up sides
  for (uint8_t i = 0; i < NumberOfFans; i++) {
    fan[i][5] = new CRGBSet( leds((i * LedsPerFan) + 5, (i * LedsPerFan)));
    fan[i][6] = new CRGBSet( leds((i * LedsPerFan) + 6, (((i + 1) * LedsPerFan) - 1)));
  }
#endif
}

//---- initSettings() --------------------------------
// Basic settings if there is nothing in EEPROM
void initSettings() {
  for (int i = 0; i < SettingGroups; i++) {
    for (int q = 0; q < SettingItems; q++) {
      gSettings[i][q] = 0;
    }
    gSettings[i][SettingItems - 1] = 6; //Last setting in a group defaults to 6 for BPM ease
  }
  gSettings[0][0] = BRIGHTNESS; //Set the initial brightness
}

#if NumberOfFans
//---- remap()----------------------------------------
// Function to remap "leds" array to "actual" array -
// Cleaned up and shrunk
void remap() {
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    if ( (i % LedsPerFan == 0) || (i % LedsPerFan == 1) ) {
      actual[i] = leds[(floor(i / LedsPerFan) * LedsPerFan) + 10 + (i % 12)];
    } else {
      actual[i] = leds[i - 2];
    }
  }
}

//---- processFans() ---------------------------------
// Primary container to step through the fans and
// call the mode function for each one
void processFans() {
  for (uint8_t i = 0; i < NumberOfFans; i++) { // Step through each fan 0 through N-1
    if (rFS(i, 0) >= NumberOfModes) {
      // gSettings[i + 1][0] = 0;  // Sanity check.
      wFS(i, 0, 0);
    }
    modefun[rFS(i, 0)](i);
  }
}
#endif

#if NumberOfStrips
//---- processStrips() ---------------------------------
// Primary container to step through the strips and
// call the mode function for each one
void processStrips() {
  for (uint8_t i = 0; i < NumberOfStrips; i++) { // Step through each strip 0 through N-1
    if (rSS(i, 0) >= NumberOfStripModes) {
      wFS(i, 0, 0); // Sanity check.
    }
    smodefun[rSS(i, 0)](i);
  }
}
#endif

//---- processGlobal() ---------------------------------
// Primary container to call the mode function for
// the whole thing
void processGlobal() {
  if (rGS(1) >= NumberOfGlobalModes) {
    wGS(1, 0); // Sanity check.
  }
  gmodefun[rGS(1)]();
}

//---- rFS(fan, setting) -----------------------------
// Read Fan Setting shorthand
// 0 through N-1 translated to setting slot number
uint8_t rFS(uint8_t i, uint8_t setting) {
  return gSettings[i + 1][setting];
}

//---- rSS(strip, setting) -----------------------------
// Read Strip Setting shorthand
// 0 through N-1 translated to setting slot number
uint8_t rSS(uint8_t i, uint8_t setting) {
  return gSettings[i + NumberOfFans + 1][setting];
}

//---- rGS(strip, setting) -----------------------------
// Read Global Setting shorthand
uint8_t rGS(uint8_t setting) {
  return gSettings[0][setting];
}

//---- wFS(fan, setting) ------------------------------
// Write Fan Setting shorthand
// 0 through N-1 translated to setting slot number
void wFS(uint8_t i, uint8_t setting, uint8_t value) {
  // i = (i > NumberOfFans) ? 1 : i + 1;
  gSettings[i + 1][setting] = value;
}

//---- wSS(strip, setting) -----------------------------
// Write Strip Setting shorthand
// 0 through N-1 translated to setting slot number
void wSS(uint8_t i, uint8_t setting, uint8_t value) {
  gSettings[i + NumberOfFans + 1][setting] = value;
}

//---- wGS(strip, setting) ----------------------------
// Write Global Setting shorthand
void wGS(uint8_t setting, uint8_t value) {
  gSettings[0][setting] = value;
}

/* RebuildGroup(grouping_setting_group)
 *  This function will set up the approppriate arrays for group
 *  remapping
 */
void RebuildGroup(uint8_t gg) {
  
}

//---- breakfast() ------------------------------------
// Consume the serial. Currently Human-readable
void breakfast() {
  // Fancy pants beat sync code. <.< ... Yeah. ^.^
  if (rGS(6) > 0) {
    static uint8_t beatCheck = 0;
    uint8_t currentBeat = beat8(rGS(6));
    if (currentBeat < beatCheck) {
      memcpy(gSettings, sSettings, SettingGroups * SettingItems);
      Serial.print("?");
    }
    beatCheck = currentBeat;
  }
  while (Serial.available()) {
    //Got to here? We have serial. Let's Do A Thing with it, one spoonful at a time.
    inByte = Serial.read();
    switch (inByte) {
      case 33 : // ! (Exclaimation point) - Save
        saveSettings();
        Serial.println("Saved");
        break;
      case 36 : // $ (Dollar sign) - Load
        loadSettings();
        Serial.println("Loaded");
        break;
      case 38 : // & (Ampersand) - Shorthand to set all the items in one group at once. MUST have all eight or will time out and/or skip other setting commands
      {         //                 &g,s0,s1,s2,s3,s4,s5,s6,s7 - Group, settings 0-7
        uint8_t rg = Serial.parseInt(); // Group
        uint8_t ig = constrain(rg, 0, (SettingGroups - 1));
        for (uint8_t q = 0; q < SettingItems; q++) {
          gSettings[ig][q] = Serial.parseInt();
        }
        break;
      }
      case 60 : // < (Less Than) - Read stage settings to live
        memcpy(gSettings, sSettings, SettingGroups * SettingItems);
        break;
      case 61 : // = (Equal) - Group Handling
        {
        /* Group Handling...
         *  Okay, yes I know that people could poke the group settings groups manually.
         *  If they do, things will break. So they shouldn't. Okee? Cool.
         */
          uint8_t rg = Serial.parseInt(); // Grouping group (1 indexed)
          uint8_t ri = Serial.parseInt(); // Index
          uint8_t iv = Serial.parseInt(); // Value
          uint8_t ig = constrain(rg, 1, MaxGroups) + NumberOfFans + NumberOfStrips;
          if (ri == SettingItems) { // Add a fan to a grouping by using setting item +1 over max item
            iv = constrain(iv, 1, NumberOfFans);
            wFS(iv, 0, 255); // Mode 255 on a fan = "Part of a group"
            wFS(iv, 1, ig);  // Set index 1 to the -setting group- of the group the fan is in
            RebuildGroup(ig);  // TO-DO: This should build the group mappings.
            break;
          } 
          if (ri == (SettingItems + 1)) { // Remove a fan from a grouping by using setting item +2 over max item
            iv = constrain(iv, 1, NumberOfFans);
            wFS(iv, 0, 7); // Sorry, prior mode is lost. STBU. Fan should be reset to desired mode after being degrouped.
            wFS(iv, 1, 0);  
            wFS(iv, 2, 0);  
            wFS(iv, 4, 0);  
            wFS(iv, 5, 128);  
            wFS(iv, 6, 3);  
            wFS(iv, 7, 30);  
            RebuildGroup(ig);  // TO-DO: This should build the group mappings.
            break;
          }
          uint8_t ii = constrain(ri, 0, (SettingItems - 1));
          gSettings[ig][ii] = iv;
          RebuildGroup(ig);
        }
        break;
      case 62 : // > (Greater Than) - Set live settings
        {
          uint8_t rg = Serial.parseInt(); // Group
          uint8_t ri = Serial.parseInt(); // Index
          uint8_t iv = Serial.parseInt(); // Value
          uint8_t ig = constrain(rg, 0, (SettingGroups - 1));
          uint8_t ii = constrain(ri, 0, (SettingItems - 1));
          /* Serial.print("OK ");
            Serial.print(ig);
            Serial.print(" ");
            Serial.print(ii);
            Serial.print(" ");
            Serial.println(iv); */
          gSettings[ig][ii] = iv;
        }
        break;
      case 63 : // ? (Question mark) - Get current settings
        for (int i = 0; i < SettingGroups; i++) {
          //Serial.print(i);
          //Serial.print(" ");
          for (int q = 0; q < SettingItems; q++) {
            Serial.print("^");
            Serial.print(i);
            Serial.print(".");
            Serial.print(q);
            Serial.print("=");
            Serial.print(gSettings[i][q]) ;
            Serial.print("  ");
          }
          Serial.println("<");
        }
        if (gMap[1][1]) {
        Serial.println(timedelta);
        }
        break;
      case 64 : // @ (at symbol) - Apply to all fans
        {
          uint8_t ri = Serial.parseInt(); // Index
          uint8_t iv = Serial.parseInt(); // Value
          uint8_t ii = constrain(ri, 0, (SettingItems - 1));
        for (int f = 1; f <= NumberOfFans; f++) {
          gSettings[f][ii] = iv;
        }
        break;
        }
      case 94 : // ^ (Caret) - Set stage settings
        {
          uint8_t rg = Serial.parseInt(); // Group
          uint8_t ri = Serial.parseInt(); // Index
          uint8_t iv = Serial.parseInt(); // Value
          uint8_t ig = constrain(rg, 0, (SettingGroups - 1));
          uint8_t ii = constrain(ri, 0, (SettingItems - 1));
          /* Serial.print("OK ");
            Serial.print(ig);
            Serial.print(" ");
            Serial.print(ii);
            Serial.print(" ");
            Serial.println(iv); */
          sSettings[ig][ii] = iv;
        }
        break;
      default :
        break;
    }
  }
}

//---- checkStorage() ---------------------------------
// Check the EEPROM storage for usability, then initialize it if necessary, and load if already initialized
void checkStorage() {
  // if (EEPROM.length() < ((SettingGroups * SettingItems) + 3)) { return; } //There is not enough EEPROM for us to use
  eeprom_read_block(eeCompat, eeHeader, 3); //Grab the header and check it.
  if (!(eeCompat[0] == SETCOMPAT && eeCompat[1] == SettingGroups && eeCompat[2] == SettingItems)) { //We don't have a valid header.
    eeCompat[0] = SETCOMPAT;
    eeCompat[1] = SettingGroups;
    eeCompat[2] = SettingItems;
    eeprom_write_block(eeCompat, eeHeader, 3);
    saveSettings();
    return;
  }
  loadSettings();
  memcpy(sSettings, gSettings, SettingGroups * SettingItems);
}

//---- saveSettings() -------------------------------
// Save the settings to EEPROM
// There SHOULD be no way to trigger this without a valid storage compatible header
// We'll see if I'm footshot for this.
void saveSettings() {
  eeprom_write_block(gSettings, eeSettings, SettingGroups * SettingItems);
}

//---- loadSettings() -------------------------------
// Load the settings from EEPROM
// There SHOULD be no way to trigger this without a valid storage compatible header
// and valid data. We'll see if I'm footshot for this.
void loadSettings() {
  eeprom_read_block(gSettings, eeSettings, SettingGroups * SettingItems);
}
