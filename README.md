
### Introduction

This demo project shows how to make use of the mach-emu sdk to emulate an arcade machine, in this case, Space Invaders.

##### Basic principles of operation

1. Determine the memory and io layout of the target application.

	See SpaceInvaders.h documentation which documents the memory and io layout for Space Invaders.

2. Write a memory and io controller which targets the desired application.

	See SpaceInvaders.cpp for a custom memory and io controller targeting Space Invaders.

3. Register these controllers with an instance of MachEmu::IMachine.

	See main.cpp for IMachine instatiation and controller registration.

4. Set the machine clock resolution for the target application.

	Space Invaders runs at 60Hz.

5. Launch a thread to run the machine.

	Once the previous prerequisites have been fulfilled, calling MachEmu::IMachine::Run() will start the machine cpu execution loop.

6. Execute the control loop.

	This handles all triggered events: audio/video rendering and quitting.

### Compilation

Space Invaders has been compiled and tested on Visual Studio 22 (Windows 10) and g++-13.2 (Ubuntu 23.10) with GNU Make 4.3.

Open cmake-gui (feel free to use command line cmake, but the remainder of this readme will use cmake-gui). Set the source code text field to the space-invaders directory and binary text field to your desire build directory.

Click configure (if prompted to create the build directory, accept) and choose Visual Studio 17 for Windows or Unix Makefiles for Linux, then click generate.

NOTE: if enableSdl is checked the SDL development package needs to be installed. 

##### Windows

The following image gives a possible Windows CMake configuration.

![Example Windows configuration](docs/images/CMake(Windows).png)

Make sure that all depdendent libraries are found (MachEmu.lib, SDl2.lib and SDL_mixer.lib), if not, make sure they are installed and their paths are
in your PATH environment variable. Update the machEmuIncludePath and the sdlIncludePath to the location of your MachEmu and SDL development includes.
Open the Visual Studio solution in the build directory, select your desired build configuration, then run.

##### Linux

The following image gives a possible Linux CMake configuration.

![Example Linux configuration](docs/images/CMake(Linux).png)

Note that the required CXX compiler needs to be g++ 13 (earlier versions may work, however this is untested) or greater (Clang is currently not supported/untested).
If the gui output displays a different compiler you can open the root CMakeLists.txt and uncomment the following line `set(CMAKE_CXX_COMPILER g++-13)`.
Once CMake has finished change into the build directory and run make. The executable will be located in bin/${configuration} in the project root directory.

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

http://www.brentradio.com/SpaceInvaders.htm<br>
https://computerarcheology.com/Arcade/SpaceInvaders/Hardware.html<br>
