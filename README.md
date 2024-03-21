
### Introduction

This demo project shows how to make use of the mach-emu sdk to emulate an arcade machine, in this case, Space Invaders.

##### Pre-requisites

The following development packages require installation before proceeding:

[mach-emu](https://github.com/nbeddows/mach-emu-dev/releases)<br>
[nlohmann_json](https://github.com/nlohmann/json/releases)

When enableSdl is checked the following development packages must also be installed:

[SDL2](https://github.com/libsdl-org/SDL/releases)<br>
[SDL2_mixer](https://github.com/libsdl-org/SDL_mixer/releases)

##### Basic principles of operation

1. Determine the memory and io layout of the target application.

	See SpaceInvaders.h documentation which documents the memory and io layout for Space Invaders.

2. Write a memory and io controller which targets the desired application.

	See SpaceInvaders.cpp for a custom memory and io controller targeting Space Invaders.

3. Register these controllers with an instance of MachEmu::IMachine.

	See main.cpp for IMachine instatiation and controller registration.

4. Set the machine options for the target application.

	Space Invaders runs at 60Hz.

5. Launch a thread to run the machine (if machine runAsync option not supported).

	Once the previous prerequisites have been fulfilled, calling MachEmu::IMachine::Run() will start the machine cpu execution loop.

6. Execute the control loop.

	This handles all triggered events: audio/video rendering, user input and quitting.

7. Wait for the machine to finish once the control loop is complete.

	The machine should be allowed to cleanup before application exit.

### Compilation

Space Invaders has been compiled and tested on Visual Studio 22 (Windows 10) and g++-13.2 and Clang 16.06 (Ubuntu 23.10) with GNU Make 4.3.

Open cmake-gui (feel free to use command line cmake, but the remainder of this readme will use cmake-gui). Set the source code text field to the space-invaders directory and binary text field to your desire build directory.

Click configure (if prompted to create the build directory, accept) and choose Visual Studio 17 for Windows or Unix Makefiles for Linux, then click generate.

##### Windows

The following image gives a possible Windows CMake configuration.

![Example Windows configuration](docs/images/CMake(Windows).png)

Make sure that all depdendent packages are found (MachEmu, SDL2, SDL_mixer and nlohmann_json) and that their library paths are
in your PATH environment variable. Update the machEmuIncludePath to the location of your MachEmu development includes.
Open the Visual Studio solution in the build directory, select your desired build configuration, then run.

##### Linux

The following image gives a possible Linux CMake configuration.

![Example Linux configuration](docs/images/CMake(Linux).png)

Make sure that all depdendent packages are found (MachEmu, SDL2 SDL_mixer amd nlohmann_json). Earlier versions of g++ and Clang may work, however these versions
are untested. Update the machEmuIncludePath to the location of your MachEmu development includes. Once CMake has finished change into the build directory and run make. The executable will be located in bin/${configuration} in the project root directory.

### Configuration

A configuration file is provided in json format which supports the following options:

`bpp`: bits per pixel, supported values are 1 (currently not supported via the SDL IO controller) and 8.<br>
`colour`: the forground colour (the background is always black), supported values are "white", "red", "green", "blue", "random" and an 8 bit custom hex value.<br>
`orientation`: the window layout, "cocktail" for horizontal and "upright" for vertical.

![Upright green 8bpp](docs/images/screenShot.png)

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

### Acknowledgements

Special thanks to the following sites:

[brentradio](http://www.brentradio.com/SpaceInvaders.htm)<br>
[computerarcheology](https://computerarcheology.com/Arcade/SpaceInvaders/Hardware.html)<br>
