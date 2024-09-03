
### Introduction

This demo project shows how to make use of the [meen](http://github.com/nbeddows/mach-emu/) and [meen_hw](http://github.com/nbeddows/meen-hw) packages to emulate an arcade machine, in this case, one based on the Space Invaders Taito/Midway arcade hardware. I don't consider the emulation to be the most efficient, accurate or to be extensively tested, but I'm happy with where it is at.

The project has been tested against the following roms (which can be found elsewhere online):

- Space Invaders.
- Space Invaders Part II/Deluxe.
- Balloon Bomber (This one looks to have issues which go beyond the superficial that require further investigation).
- Lunar Rescue.

### Compilation

This project uses [CMake (minimum version 3.23)](https://cmake.org/) for its build system and [Conan (minimum version 2.0)](https://conan.io/) for it's dependency package management. Supported compilers are GCC (minimum version 12), MSVC(minimum version 16).

#### Pre-requisites

##### Linux

- [Install Conan](https://conan.io/downloads/).
- `sudo apt install cmake`.
- cross compilation:
  - armv7hf:
    - `sudo apt install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf`.
  - aarch64:
    - `sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu`.

##### Windows

- [Install Conan](https://conan.io/downloads).
- [Install CMake](https://cmake.org/download/).

**1.** Install the supported meen conan configurations (v0.1.0) (if not done so already):
- `conan config install -sf profiles -tf profiles https://github.com/nbeddows/meen-conan-config.git --args "--branch v0.1.0"`

**2.** Install dependencies:
- Windows msvc x86_64 build and host: `conan install . --build=missing --profile:build=Windows-x86_64-msvc-193 --profile:host=profiles/Windows-x86_64-msvc-193-sdl`.
- Linux x86_64 build and host: `conan install . --build=missing --profile:build=Linux-x86_64-gcc-13 --profile:host=profiles/Linux-x86_64-gcc-13-sdl`.
- Linux x86_64 build, Linux armv7hf host: `conan install . --build=missing -profile:build=Linux-x86_64-gcc-13 -profile:host=profiles/Linux-armv7hf-gcc-13-sdl`.
- Linux x86_64 build, Linux armv8 host: `conan install . --build=missing -profile:build=Linux-x86_64-gcc-13 -profile:host=profiles/Linux-armv8-gcc-13-sdl`.

**NOTE**: when performing a cross compile using a host profile you must install the requisite toolchain of the target architecture, [see pre-requisites](#pre-requisites).

**NOTE**: under Linux with an sdl host profile errors similar to the following, `ERROR: xorg/system: Error in system_requirements() method` require additional package installations as denoted by the above console messages: "`dpkg-query: no packages found matching ${pkg0}`": `sudo apt install ${pkg0} ${pkg1} ${pkgn}`.
When cross compiling for arm you may need to add the arm development repositories to your apt sources if the packages previously installed could not be found, for example (at the time of writing):
- `sudo nano /etc/apt/source.list`
- Append the following:
    - deb [arch=arm64] http://ports.ubuntu.com/ lunar main multiverse universe
    - deb [arch=arm64] http://ports.ubuntu.com/ lunar-security main multiverse universe
    - deb [arch=arm64] http://ports.ubuntu.com/ lunar-backports main multiverse universe
    - deb [arch=arm64] http://ports.ubuntu.com/ lunar-updates main multiverse universe
- Save and exit
- `sudo dpkg --add-architecture arm64`
- `sudo dpkg --print-foreign-architectures`
- `sudo apt-get update`
- Reinstall the missing packages.

The following dependent packages will be (compiled if required and) installed:

- mach_emu
- meen_hw
- nlohmann_json
- popl
- sdl
- sdl_mixer

**3.** Run cmake to configure and generate the build system.

- Multi configuration generators (MSVC for example): `cmake --preset conan-default [-Wno-dev]`.
- Single configuration generators (make for example): `cmake --preset conan-release [-Wno-dev]`.

**4.** Run cmake to compile i8080-arcade: `cmake --build --preset conan-release`.

**5.** Run i8080-arcade:

**Linux/Windows (x86_64)**:
- `build\generators\conanrun.bat/sh`: export the dependent shared library paths.
- `artifacts/Release/x86_64/bin/i8080-arcade`.
- `build\generators\deactivate_conanrun.bat/sh`: restore the environment.

**Linux (armv7hf, armv8)**:

When running a cross compiled build the binaries need to be uploaded to the host machine before they can be executed.
1. Create an Arm Linux binary distribution: See building a binary package. 
2. Copy the distribution to the arm machine: `scp build/Release/Sdk/i8080-arcade-v0.6.0-Linux-armv7hf-bin.tar.gz ${user}@raspberrypi:i8080-arcade-v0.6.0.tar.gz`.
3. Ssh into the arm machine: `ssh ${user}@raspberrypi`.
4. Extract the i8080-arcade archive copied over via scp: `tar -xzf i8080-arcade-v0.6.0.tar.gz`.
5. Change directory to i8080-arcade `cd i8080-arcade`.
6. Run i8080-arcade: `./run-i8080-arcade.sh`.<br>

The following command line options are available:

- `-h, --help`: display a help message listing all available command line options.
- `-c, --config-file`: the configuration file to load (default: conf/config.json).
- `-r, --rom-file-path`: the path to the rom files directory (default: rom-files).
- `-a, --audio-file-path`: the path to the audio samples directory (default: audio-files).
- `-s, --save-file-path`: the path to the save files directory (default: save-files).
- `-g, --game`: the name of the i8080 arcade game to load as defined in the config file (default: space-invaders).

#### Building a binary package

A standalone binary package can be built via `cpack` that can be distributed and installed:

- `cmake --build --preset conan-release --target=i8080-arcade-pkg`

The `i8080-arcade-pkg` target defined in the root CMakeLists.txt will build a tar gzipped package via the following cpack command:

- `cpack --config build\CPackConfig.cmake -C ${buildType} -G TGZ`

The underlying package generator used to build the package (in this case `tar`) must be installed otherwise this command will fail.

NOTE: the `-G` option can be specifed to overwrite the default `TGZ` cpack generator if a different packaging method is desired:

- `cpack --config build\CPackConfig.cmake -C ${buildType} -G ZIP`

This will build a binary package using the `zip` utility.

Run `cpack --help` for a list available generators.

The `i8080-arcade-pkg` target will also strip all binary files where applicable and will also perform static analysis of the source code via `cppcheck`.

### Configuration

A configuration file targeting the i8080 arcade hardware is provided in json format. It is designed for flexibility and verbosity. It is divided into two main sections:

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

##### Space Invaders/Space Invaders Deluxe/Space Invaders II/Balloon Bomber/Lunar Rescue

These settings are fixed to the specified rom and should not be changed.

`memory:rom:file:name` - The name of the rom file.<br>
`memory:rom:file:offset` - The start of memory rom load offset.<br>
`memory:rom:file:size` - The rom file size.<br>
`memory:ram:block:offset` - The start of memory ram block offset.<br>
`memory:ram:block:size` - The ram block size.<br>

### Keyboard Controls

`q`: Quit<br>
`c`: Credit<br>
`1`: 1P<br>
`2`: 2P<br>
`a`: 1P left<br>
`s`: 1P fire<br>
`d`: 1P right<br>
`3`: 3 ships<br>
`4`: 4 ships<br>
`5`: 5 ships<br>
`6`: 6 ships<br>
`t`: Tilt<br>
`e`: Extra ship at<br>
`j`: 2P left<br>
`k`: 2P fire<br>
`l`: 2P right<br>
`i`: Show coin info<br>
`y`: Save game<br>
`r`: Load game<br> 

![space-invaders](docs/images/space-invaders.png) ![space-invaders-deluxe](docs/images/space-invaders-deluxe.png) ![lunar-rescue](docs/images/lunar-rescue.png) ![balloon-bomber](docs/images/balloon-bomber.png)

### Acknowledgements

Special thanks to the following sites:

[brentradio](http://www.brentradio.com/SpaceInvaders.htm)<br>
[computerarcheology](https://computerarcheology.com/Arcade/SpaceInvaders/Hardware.html)<br>
