# HD120-Controller
### Arduino sketch to act as a custom controller for Corsair HD120 RGB Fans

Copyright 2017 Kit Parenteau

Version score: 0.2.1.1

The lighting controller that comes standard for HD120 RGB fans left something to be desired.
This code allows a 5V Arduino AVR to be used as a controller for the fan lighting.
* Tested on Arduino Pro Micro
* FastLED Library 3.1 or newer, and Arduino.cc IDE 1.8.1 or newer are required to compile

This is NOT the same software used to make the Prototype video. Version 0.2.0.0 was used to make the [music demo](https://www.youtube.com/watch?v=9Bf8jwD-sW4). The prototype video was just the 100-line demo from FastLED.

Consider this to be an ALPHA version. Some assembly required. Do not taunt code. Comments may cause sweating or blindness.

User Features
* Software controlled (Currently by Serial communications)
* Control individual fans.
* Global Brightness Control
* Multiple Modes (And more to come)
* Timing Sync Mode

Coder Features
* Remapping of the fans to put the first LED in the proper place
* Mapping per sides of fans
* Mapping per quadrant of fan
* Uses FastLED - 8-bit mapped stuff makes things easy


-- NOT UP TO DATE FOR 0.2.0.0 - Refer to code comments until this is fully updated.
### Controls:
Open a serial connection to the Arduino com port at 115200 Baud
- For example, use Serial monitor or command line redirect to the serial port
- If using serial monitor, use Newline line ending setting

Commands can be strung together in one send chunk, however the refresh will pause while the controller is parsing commands. This is generally very fast. At 115,200 baud, it takes about 1ms to send every 12 characters. Command strings exceeding 150 characters may cause one refresh to be delayed, as they normally occur sixty times per second.

Commands consist of a header character followed by any necessary options.

 Header Character | Meaning 
 ---: | :--- 
\> | Set live
^ | Set stage
< | Transfer stage to live
! | Save settings to memory
$ | Load settings from memory
? | Query current live settings
@ | Set all fans (Only applies to fans, not strips)

Any character that is not part of a continuing command or a header will be discarded and disregarded.

#### Header Commands
##### Set Live

A greater than symbol followed by three integers with arbitrary separation. We recommend you use single-character separation to avoid excess command read time. This sets directly into the live settings and takes effect immediately.

The integers, in order, are Setting Group, Setting Item, Setting Value. Examples:
```
>0,0,255    --> Sets group 0, item 0 to 255
>1.0=6      --> Sets group 1, item 0 to 6
>1.0.0>1.1.128>1.2.255    --> Sets three settings at once
```

Default groups:
0    -> Global
1-6  -> Each fan
7-10 -> Each strip

Group 0 is always global. Then zero or more fan groups. Then zero or more strip groups.

 -- Global Settings 
 0   = Remembered Global Brightness
 1   = Global Mode
 2-5 = Global Mode Data
 6   = Sync timing rate
 7   = Global timing rate

Fan settings:
0   -> Mode
1-7 -> Mode-dependent

Modes:
0 -> Angry Myia Mode - Cycle from teal to red and back, approx 10 sec per cycle
Settings: 
None. Not-configurable.
ToDo: Consider confiiguration options.

1 -> Static Color
Settings:
1 -> Hue (FastLED 0-255 Hue, not 0-360. See Note 1 below.)

2 -> Single Color Pulse
Settings:
1 -> Hue
2 -> BPM (60/BPM = Time to pulse)

3 -> Rotating full rainbow per fan
Settings:
None.
ToDo: Consider direction/speed

4 -> Cycle full fan through rainbow and back in ten seconds
Settings:
None.
ToDo: Consider BPM options.

5 -> Rainbow Sparkles - Same as 3 Plus Sparkles
Settings:
1 -> Chance per frame of a sparkle, 0-255

Note 1:
FastLED Hue Chart is [Here](https://raw.githubusercontent.com/FastLED/FastLED/gh-pages/images/HSV-rainbow-with-desc.jpg)
0   -> Red
32  -> Orange
64  -> Yellow
96  -> Green
128 -> Aqua/Teal
160 -> Blue
192 -> Purple
224 -> Pink
255 -> Red


Changelog
0.2.1.1 - May 13, 2017
* IMPROVED: Removed need for group number in All-Fan prefix

0.2.1.0 - May 13, 2017
* FIXED: Will no longer have severe timing issues when NumberOfStrips is set to 0
* ADDED: Use @ prefix to change a setting item on all fans at once. Ignores group number
* * TODO: Remove need for group number
* IMPROVED: Updated delay calculation logic to accoint for the number of LEDs.

0.2.0.0 - April 23, 2017
* ADDED: EEPROM Saving and Loading
* ADDED: Beat Sync Control
* * Global Sync Mode
* * Staging Settings
* ADDED: Numerous new fan modes
* ADDED: Strip control on separate data pin
* ADDED: Global modes
* MODIFIED: Consolidated some fan modes down into one. For example, "single hue color" is the same as two sides with both sides having the same color.
* ADDED: Ability to query current settings. Results are also in staging mode format and can be pasted back to the serial port to stage them.
* IMPROVED: Some code flow for end binary size.

0.1.1.0 - February 6, 2017
* ADDED: Two more fan modes
* ADDED: Local compile size tracking in comments
* IMPROVED: Cleaned up code and increased comment clarity
