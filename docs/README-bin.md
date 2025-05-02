
### Introduction

This is a binary distribution of an [emulated i8080 arcade machine](https://github.com/nbeddows/i8080-arcade) based on the Space Invaders Taito/Midway arcade hardware using [meen](http://github.com/nbeddows/mach-emu/) and [meen-hw](http://github.com/nbeddows/meen-hw/).

It adds the following addtional graphical components to the emulation:
- a rom selection screen allowing the user select and load the supported roms (see controls section for further details).  
- title and credits.
- metadata including: frame rate, time, up time and memory usage.

This emulator has been tested against the following roms (which can be found elsewhere online):

- Space Invaders.
- Space Invaders Part II/Deluxe.
- Balloon Bomber (This one looks to have issues which go beyond the superficial that require further investigation).
- Lunar Rescue.

For supported desktop platforms The Simple Direct MediaLayer (SDL) is used to render the output and requires a keyboard for interaction (keyboard controls are documented towards the end of this document).
For supported embedded platforms an st7789 based lcd screen is required for rendering the output (tested with 320x240) with a minimum of 4 buttons for interaction (button controls are documented towards the end of this document).

I don't consider the emulation to be the most efficient, accurate or to be extensively tested, but I'm happy with where it is at.

### Running the application

A script is provided in the root directory which will configure the environment and run the i8080 arcade machine.

`./run-i8080-arcade.sh`

### Configuration

A configuration file targeting the i8080 arcade hardware is provided in json format. It is designed for flexibility and verbosity.

It is divided into two main sections:

#### Hardware

These options should be fixed to the specfied values unless stated otherwise.

##### Meen

The current settings for these options should be sufficient, changing them may have a negative impact on performance.

`clockSamplingFreq:120` - i8080 arcade hardware runs at 60Hz with 2 interrupts per frame.<br>
`isrFreq:132` - We require 4 interrupts, 2 for i8080-arcade and 2 machine level interrupts for loading and saving. Ideally we would lock the interrupt service routine frequency to the clock sampling frequency ("isrFreq":120), however, we need to spare some time for checking for load and save requests, so we bump the isrFreq up by ten percent ("isrFreq":132). One could increase it further (increased host cpu usage), this would make it more responsive (132 should be good enough).<br>
`loadAsync:true` - Load the machine state asynchronously.<br> 
`runAsync:true` - Run the machine asynchronously from the io.<br>
`saveAsync:true` - Save the machine state asynchronously.<br>

**NOTE**: the RP IO Controller does not support saving state.
**NOTE**: running in synchronous mode (`runAsync` = `false`) is supported for demonstration purposes, however, it should be left to `true` for performance reasons.
**NOTE**: running in synchronous mode (`runAsync` = `false`) requires an increase in the `isrFreq` parameter for improved responsiveness (240 recommended).

##### Video

Video hardware options. These options can be changed for the desired output.

`width:320` - The width of the screen in pixels. For upright orientation, the minimum support width of the display is 240, for cocktail, 320.<br>
`height:240` - The height of the screen in pixels. For upright orientation, the minimum supported height of the display is 320, for cocktail, 240.<br>
`fullScreen:false` - Window or full screen display. (Experimental)<br>

**NOTE**: the RP IO Controller does not support scaling or full-screen, the width and height parameters will be used to center the output on the display device.

##### Audio

Audio hardware options. The current settings for these options should be sufficient.

`channels:1` - The number of audio output channels.<br>
`sampleRate:11025` - The audio output sample rate.<br>
`sampleSize:512` - The audio output sample size.<br>

**NOTE**: these options can be changed if using custom audio samples.
**NOTE**: the RP IO Controller does not support audio, these options have no affect.

#### Software

These settings apply to the various arcade roms that can be loaded.

##### Video

These settings affect visual output and can be changed. They apply to all game roms loaded.

`bpp:16` - Bits per pixel, supported values are 1 (experimental and not universally supported), 8 (rgb332) and 16 (rgb565).<br>
`colour:white` - The forground colour (the background is always black), supported values are "white", "red", "green", "blue", "random" and a 16 bit custom hex value.<br>
`orientation:cocktail` - The window layout, "cocktail" for horizontal and "upright" for vertical.<br>

**NOTE**: the RP IO Controller only supports cocktail orientation @ 16bpp.

##### Audio

These settings affect audio output. They can be changed if different audio samples are desired. They apply to all game roms loaded.

`audio:file` - The name of the audio sample to load (empty entries are ignored and **must** not be removed).<br>

**NOTE**: the position of the audio files in the array **must** not be changed.<br>
**NOTE**: if changing the audio files, the audio hardware properties may need to be updated (untested).<br>
**NOTE**: the RP IO Controller does not support audio, these setting have no affect.

##### Space Invaders/Space Invaders Deluxe/Space Invaders II/Balloon Bomber/Lunar Rescue

These settings are fixed to the specified rom.

`roms:name` - The name of the rom. This is used as the name of the save state json file.<br>
`roms:cpu:pc` - The meen cpu program counter. It **must** not be changed, doing so will yield undefined behaviour.<br>
`roms:cpu:sp` - The meen cpu stack pointer. It **must** not be changed, doing so will yield undefined behaviour.<br>
`memory:rom:scheme` - An optional parameter specifying the type of the rom resource to load, either `file://` or `json://`.<br>
`memory:rom:directory` - An optional parameter specifying the path to the rom resource to load.<br>
`memory:rom:[block]:bytes`: The rom resource to load. When the resource is fully qualified it will ignore the scheme and directory parameters.
`memory:rom:[block]:offset`: The offset into memory where the rom will be loaded, this value **must** not be changed, doing so will yield undefined behaviour.<br>

**NOTE**: The rom name is used for the entry in the rom selection screen. It **must** not have any new line characters and **must** only contain characters defined in the supported font defined in `GlyphRenderer.cpp`. The maximum number of characters supported per entry is 26 ((the vram width (224) / the supported font width (8)) - 2 spaces (one is prepended and one is appeneded to the name)). 

### Desktop Keyboard Controls

`q`: quit<br>
`c`: credit<br>
`1`: 1P<br>
`2`: 2P<br>
`a`: 1P left<br>
`s`: 1P fire<br>
`d`: 1P right<br>
`3`: 3 ships<br>
`4`: 4 ships<br>
`5`: 5 ships<br>
`6`: 6 ships<br>
`t`: tilt<br>
`e`: extra ship at<br>
`j`: 2P left<br>
`k`: 2P fire<br>
`l`: 2P right<br>
`i`: show coin info<br>
`y`: save game<br>
`r`: load save game<br>
`up`: move to the prevous rom<br>
`down`: move to the next rom<br>
`enter`: load the selected rom<br>
`esc`: return to the rom selection screen<br> 

### Embedded button controls

Four buttons can be used to control a one player emulation. All game settings will be at defaults, this includes starting with 3 ships with an addtional
ship every 1500 points.

More buttons and/or logic can be added for a more complete emulation, 2 player support for example (keeping it simple for demonstration purposes).

The pin layout used is the same as the [hardware lcd](https://www.waveshare.com/wiki/Pico-LCD-2) used for testing.

`button 0`: when on the loaded roms attraction screen, start a 1P game, when in gameplay mode, move the ship to the left.
`button 1`: when in gameplay mode, quit and return to the rom selection screen.
`button 2`: when on the rom selection screen, select the previous rom in the list of supported roms, when in gameplay mode, fire at the enemy.
`button 3`: when on the rom selection screen, select the next rom in the list of supported roms, when in gameplay mode, move the ship to the right.

![space-invaders](docs/images/space-invaders.png) ![space-invaders-deluxe](docs/images/space-invaders-deluxe.png) ![lunar-rescue](docs/images/lunar-rescue.png) ![balloon-bomber](docs/images/balloon-bomber.png)

### Acknowledgements

Special thanks to the following sites:

[brentradio](http://www.brentradio.com/SpaceInvaders.htm)<br>
[computerarcheology](https://computerarcheology.com/Arcade/SpaceInvaders/Hardware.html)<br>