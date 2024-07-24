
### Introduction

This is a binary distribution of an [emulated i8080 arcade machine](https://github.com/nbeddows/i8080-arcade) based on Space Invaders arcade hardware using [mach-emu](http://github.com/nbeddows/mach-emu/) and [meen-hw](http://github.com/nbeddows/meen-hw/).

### Running the application

A script is provided in the root directory which will configure the environment and run the i8080 arcade machine.

`./run-i8080-arcade.sh`

### Configuration

A configuration file targeting the i8080 arcade hardware is provided in json format. It is desinged for flexibility and verbosity. It is divided into two main sections:

#### Hardware

These options should be fixed to the specfied values unless stated otherwise.

##### MachEmu

The current settings for these options should be sufficient, changing them may have a negative impact on performance.

`clockResolution:1000000000 / 60 / 2` - i8080 arcade hardware runs at 60Hz with 2 interrupts per frame, set the machine clock resolution accordingly.<br>
`isrFreq:0.9` - We require 4 interrupts, 2 for i8080-arcade and 2 machine level interrupts for loading and saving. Ideally we would lock the interrupt service routine frequency to the clock resolution ("isrFreq":1), however, we need to spare some time for checking for load and save requests, so we bump the isrFreq down by ten percent ("isrFreq":0.9). One could lower it further, this would make it more responsive (0.9 should be good enough). Increasing it above 1 would make it slower and not respond to load/save requests.<br>
`loadAsync:true` - Load the machine state asynchronously.<br> 
`runAsync:true` - Run the machine asynchronously from the io.<br>
`saveAsync:true` - Save the machine state asynchronously.<br>

##### Video

Video hardware options. These options can be changed for the desired output.

`width:224` - The width of the screen.<br>
`height:256` - The height of the screen.<br>
`full-screen:false` - Window or full screen display.<br>

##### Audio

Audio hardware options. The current settings for these options should be sufficient.

`channels:1` - The number of audio output channels.<br>
`sample-rate:11025` - The audio output sample rate.<br>
`sample-size:512` - The audio output sample size.<br>

**NOTE**: these options can be changed if using custom audio samples.

#### Software

These settings apply to the various arcade roms that can be loaded.

##### Video

These settings affect visual output and can be changed. They apply to all game roms loaded.

`bpp:8` - Bits per pixel, supported values are 1 (currently not supported via the SDL IO controller) and 8.<br>
`colour:white`: the forground colour (the background is always black), supported values are "white", "red", "green", "blue", "random" and an 8 bit custom hex value.<br>
`orientation:upright` - The window layout, "cocktail" for horizontal and "upright" for vertical.<br>

##### Audio

These settings affect audio output. They can be changed if different audio samples are desired. They apply to all game roms loaded.

`audio:file` - The name of the audio sample to load (empty entries are ignored and **must** not be removed).<br>

**NOTE**: the position of the audio files in the array **must** not be changed.<br>
**NOTE**: if changing the audio files, the audio hardware properties may need to be updated (untested).

##### Space Invaders/Space Invaders Deluxe/Balloon Bomber/Lunar Rescue

These settings are fixed to the specified rom and should not be changed.

`memory:rom:file:name` - The name of the rom file.<br>
`memory:rom:file:offset` - The start of memory rom load offset.<br>
`memory:rom:file:size` - The rom file size.<br>
`memory:ram:block:offset` - The start of memory ram block offset.<br>
`memory:ram:block:size` - The ram block size.<br>

### Keyboard Controls

`q`: Quit<br>
`c`: Credit<br>
`1`, 1P<br>
`2`: 2P<br>
`a`: 1P Left<br>
`s`: 1P Fire<br>
`d`: 1P Right<br>
`3`: 3 Ships<br>
`4`: 4 Ships<br>
`5`: 5 Ships<br>
`6`: 6 Ships<br>
`t`: Tilt<br>
`e`: Extra ship at<br>
`j`: 2P Left<br>
`k`: 2P Fire<br>
`l`: 2P Right<br>
`i`: Show coin info<br>

![Upright green 8bpp](docs/images/screenShot.png)

### Acknowledgements

Special thanks to the following sites:

[brentradio](http://www.brentradio.com/SpaceInvaders.htm)<br>
[computerarcheology](https://computerarcheology.com/Arcade/SpaceInvaders/Hardware.html)<br>
