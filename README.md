NOTE: 2.1.3 may cause the controller to not respond properly to soft resets to bootloader. This applies to Leonardo type ATMega32u* controllers only, like the Pro Micro and Beetle. If you are unable to upload this new version because you went to 2.1.3, there are two options to recover it:
1: Send an initial soft reset just before the upload starts. The double soft reset corrects the issue and properly enters bootloader. This can be done by opening the com port at 1200 baud and closing it. For example, on Windows in the command prompt:
```
mode COM6 BAUD=1200 parity=N data=8 stop=1
```
Replace COM6 with the com port of the Arduino.

Timing is important, as the second reset from the Arduino IDE must reeach the Arduino after it finished the first reset (about 500ms) and before 3 seconds after the first reset.

2: You can do a hardware or double-soft reset to put it into the bootloader and use avrdude to send the firmware.

You MUST know the port the Ardiono Bootloader will be on. It is NOT the same com port as the normal run port. You can find out in Windows by watching Computer Management in the Ports section of Device Manager for the proper port number.

To do this, you must compile and SAVE the firmware (You can do this by pressing Ctrl-Alt-S on Windows IDE). On Windows, the firmware will be saved under Documents\Arduino\(Your Sketch)\sketchname.ino.board.hex. For example:
```
C:/Users/kit/Documents/Arduino/HD120_Controller/HD120_Controller.ino.leonardo.hex
```

DO NOT use the one that mentions "boot" in the filename.

On Windows in a command prompt, change directory to:
```
C:\Program Files (x86)\Arduino\hardware\tools\avr\bin
```

And run a command similar to:
```
avrdude -v -C../etc/avrdude.conf -patmega32u4 -cavr109 -PCOM9 -b57600 -D -V -Uflash:w:C:/Users/kit/Documents/Arduino/HD120_Controller/HD120_Controller.ino.leonardo.hex:i 
```

Replace only the location and name of the .hex file and the COM port number with information appropropriate to your computer.

You can do a hardware reset by temporarily connecting the RST pin to ground.
You can use the mode command above twice to force a soft reset.

I will work on a more user-friendly way to fix this in the near future if I am able to.

# HD120-Controller
### Arduino sketch to act as a custom controller for Corsair HD120 RGB Fans

Copyright 2018 Kit Parenteau with code contributed by Bud Griffin

Version score: 0.2.1.3 + Fan Type - Now handles Mixed HD and LL fans!

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
& | Set full group

Any character that is not part of a continuing command or a header will be discarded and disregarded.

#### Header Commands
##### Set live

A greater than symbol followed by three integers with arbitrary separation. We recommend you use single-character separation to avoid excess command read time. This sets directly into the live settings and takes effect immediately.

The integers, in order, are Setting Group, Setting Item, Setting Value. Examples:
```
>0,0,255    --> Sets group 0, item 0 to 255
>1.0=6      --> Sets group 1, item 0 to 6
>1.0.0>1.1.128>1.2.255    --> Sets three settings at once
```

##### Set stage

A karet symbol followed by three integers with arbitrary separation. We recommend you use single-character separation to avoid excess command read time. This sets into the stage settings and must be transferred to live to take effect.

The integers, in order, are Setting Group, Setting Item, Setting Value. Examples:
```
^0,0,255    --> Sets group 0, item 0 to 255
^1.0=6      --> Sets group 1, item 0 to 6
^1.0.0^1.1.128^1.2.255    --> Sets three settings at once
```

##### Transfer stage to live

A less than symbol. No further input needed. Transfers all settings currently on stage to live settings. This also occurs automatically immediately before a sync ping. This allows settings to be configured and then put into play simultaneously.

##### Save settings to memory

An exclamation point. No further input needed. Saves all live settings to the EEPROM on the controller. 

##### Load settings from memory
A dollar sign. No further input needed. Loads settings from EEPROM on the controller. Also occurs automatically on startup if the setting save area is compatible with the current configuration, otherwise reinitializes the save area.

##### Query live settings

A question mark. No further input needed. Sends the current live settings across the serial connection. The settings are formatted in such a way that they can be sent back to set them.

##### Set All Fans

An at symbol followed by two integers with arbitrary separation. Shortcut to set a given Item to a given Value on all fans simultaneously. Sets LIVE values. Only affects fans, not strips.

##### Set All Fans

An ampersand symbol followed by nine integers with arbitrary separation. Shortcut to set all of the items in one group at a time. MUST have group number and all eight values otherwise it will time out and/or cause chaos.

#### Default groups:

Group | Purpose
---: | :--- 
0    | Global
1-6  | Each fan
7-10 | Each strip

Group 0 is always global. Then zero or more fan groups. Then zero or more strip groups.

##### Global Settings 

Setting | Purpose
---: | :--- 
0   | Remembered Global Brightness
1   | Global Mode
2-5 | Global Mode Data
6   | Sync timing rate - Special Item
7   | Global timing rate

###### Global Modes
0 ==> Nothing. No Global Override

1 ==> Pulse brightness globally between 0 and Max Brightness 

Setting | Purpose
---: | :--- 
7 | Rate of Pulse

2 ==> Flash one fan a color. This will unset after flashing, causing the normal fan actions to continue. Meant for sync special effects. Not for strips.

Setting | Purpose
---: | :--- 
2 | Fan number to flash
3 | Red channel value
4 | Green channel value
5 | Blue channel value

##### Sync Timing
When global mode item 6 is set to a rate above zero, the controller will enter Sync Timing Mode. At the speed of the rate, it will move stage settings to live, then send a "Ping" across Serial, consisting of a single question mark. Stage settings can be configured by the computer immediately after receiving a ping and they will be set to live on the next beat. WARNING: The serial port MUST be read by the computer when Sync Timing is enabled otherwise the serial buffer on the controller will overflow and hamper performance severely.

##### Fan settings:

Setting | Purpose
---: | :--- 
0   | Mode
1-7 | Mode-dependent

Mode-Dependant Settings are 0-255 unless otherwise indicated
Rates are in BPM (60/Rate seconds per cycle)

Modes:
0 ==> Hue Shift - Sine wave color change of all LEDs on the fan between two hues.

Setting | Purpose
---: | :--- 
1 | Starting Hue
2 | Emding Hue
3 | Hue Offset
4 | Starting Saturation
5 | Phase Offset (Note 2)
6 | Ending Saturation
7 | Rate

Hue Offset allows passing through Red between hues. For example, Start 128 End 255 Phase Offset 64 will start at 192 and end at 64. The use of saturation values is recommended. 0 is white. 255 is full color.

1 ==> Single-point  Spinner - A rotating light point.

Setting | Purpose
---: | :--- 
1 | Hue (Overridden by 3)
2 | 0 = Clockwise, 1+ = Counterclockwise
3 | 0 = Use Hue; 1 = Rainbow Mode - Runs through all hues - Overrides 1; 2 = Sparkler Mode
4 | Rate of rainbow change
5 | Blade Offset 0 - 11 (zero through number of leds per fan) - Offsets the position of the "blade" dot
6 | Fade Speed - After the dot leaves a LED, fade its trail
7 | Spin Rate


2 ==> Rainbow span across LEDs

Setting | Purpose
---: | :--- 
1 | Chance of Sparkles - Very high will be a 60 FPS fade to white for the fan.
2 | Hue Steps per LED - 21 shows a full rainbow on the fan, 0 causes the whole fan to fade colors at once
3 | Fan Hue Offset
4 | 0 = Normal Rotation; 1 = Reverse Rotation
7 | Rate of rainbow rotation - 0 is static

3 ==> Four-point spinner

Setting | Purpose
---: | :--- 
1 | Hue (Overridden by 3)
2 | 0 = Clockwise, 1+ = Counterclockwise
3 | 0 = Use Hue; 1 = Rainbow Mode - Runs through all hues - Overrides 1; 2 = Sparkler
4 | Rate of rainbow change
5 | Per Blade Hue Shift
6 | Fade Speed - After the dot leaves a LED, fade its trail
7 | Spin Rate

4 ==> Double-Scan - Back and forth patter across two sides of the fan

Setting | Purpose
---: | :--- 
1 | Hue (Overridden by 3)
2 | Rotation Offset = 0 - 11 (Zero through LedsPerFan - 1)
3 | 0 = Use Hue; 1 = Rainbow Mode - Runs through all hues - Overrides 1; 2 = Sparkler
4 | Rate of rainbow change
5 | Per Blade Hue Shift
6 | Fade Speed - After the dot leaves a LED, fade its trail
7 | Spin Rate

5 ==> Two-point spinner

Setting | Purpose
---: | :--- 
1 | Hue (Overridden by 3)
2 | 0 = Clockwise, 1+ = Counterclockwise
3 | 0 = Use Hue; 1 = Rainbow Mode - Runs through all hues - Overrides 1; 2 = Sparkler
4 | Rate of rainbow change
5 | Per Blade Hue Shift
6 | Fade Speed - After the dot leaves a LED, fade its trail
7 | Spin Rate

6 ==> "BPM" Mode from FastLED 100-Line Demo - Because somebody asked for it. Whatev's.

Setting | Purpose
---: | :--- 
1 | Hue Multiplier
2 | Beat Multiplier
7 | Rate

Honestly, nobody knows much about this one.

7 ==> Split Sides

Setting | Purpose
---: | :--- 
1 | West Side Hue
2 | East Side Hue
4 | Fan pulse phase offset
5 | Per side pulse phase offset
6 | 0 = No pulse; 1 = Sine pulse; 2 = Sawtooth In Pulse; 3 = Sawtooth Out Pulse; 4+ Triangle Wave
7 | Pulse Rate
- Both sides can be set to the same hue to pulse a color on the full fan

8 ==> Split Quarters

Setting | Purpose
---: | :--- 
1 | Northwest Side Hue
2 | Northeast Side Hue
3 | Southeast Side Hue
4 | Southwest Side Hue
5 | Per side pulse phase offset
6 | 0 = No pulse; 1 = Sine pulse; 2 = Sawtooth In Pulse; 3 = Sawtooth Out Pulse; 4+ Triangle Wave
7 | Pulse Rate

9 ==> Full Fan RGB Color

Setting | Purpose
---: | :--- 
1 | Red Level
2 | Green Level
3 | Blue Level
- People wanted white.

10 ==> Fade to Black - Will fade at the fade rate and then do nothing more until changed. Useful for sync.

Setting | Purpose
---: | :--- 
1 | Fade Rate


##### Note 1:

FastLED Hue Chart is [Here](https://raw.githubusercontent.com/FastLED/FastLED/gh-pages/images/HSV-rainbow-with-desc.jpg)
- 0   -> Red
- 32  -> Orange
- 64  -> Yellow
- 96  -> Green
- 128 -> Aqua/Teal
- 160 -> Blue
- 192 -> Purple
- 224 -> Pink
- 255 -> Red

##### Note 2:
Phase Offset applies to some Sine-wave situations. The value for a given rate is always the same. Phase offset creates a 0-255 offset from the base value. This allows, for example, two fan LED sets to operate in reverse of each other at the same rate.

##### Extra Note:
Several interesting effects can be created with various values in the modes. Usually changing a value by a small amount has little effect, but some values can be dramatic.


Changelog
0.2.2.0 - March 1, 2018
* IMPROVED: Memory use reduced by in situ shuffling of remap that no longer requires a second array
* FIXED: Sketch caused Arduino to no longer respond correctly to reset.

0.2.1.3 - February 8, 2018
* ADDED: Mixed LL/HD fan handling - Many thanks to Bud Griffin!
* ADDED: Sparkler mode to several modes
* FIXED: Invalid Strip modes will no longer cause a controller crash
* ADDED: Use the & prefix to address a whole group at once
* ADDED: Saturation Start/End to Hue Shift mode to allow pastels and white
* IMPROVED: Converted segment handling to reduce memory footprint, allowing more than eight fans and four strips as an example
* FORGOT: Documentation on Strip Modes, which are getting more interesting
* REMOVED: Several old, commented code segments and squishy comments


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
