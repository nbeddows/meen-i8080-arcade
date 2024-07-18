0.6.0
* Using meen-hw for i8080-arcade support.

0.5.0 [24/06/24]
* Updated to mach-emu-1.6.1.
* Using Conan package manager.
* Added CPack support for binary distributions.
* Updated the JSON config file for improved flexibility.

0.4.0 [1/05/24]
* Updated to mach-emu-1.5.1.
* Added `IMachine::Load` support.
* Added `IMachine::Save` support.
* Added support for Space Invaders Part II Deluxe.
* Moved all required options to the config file.
* Updated the README to document the config options.

0.3.1 [15/04/24]
* Fixed a deadlock on quit when the interrupt service routine
  frequency is greater than 0.

0.3.0 [21/03/24]
* Updated to mach-emu-1.4.0.
* Process keyboard input on the main thread.
* Minor documentation updates.

0.2.0 [13/02/24]
* Added a json file for config options.
* Added native 1bpp pixel format.
* Added native horizontal video frame output.
* Added colour support.
* SDL2 and SDL2_mixer are now located from CMake
  via find_package.
* Updated documentation.

0.1.1 [07/02/24]
* Added a frame pool to prevent superfluous memory
  allocations.
* Improved audio event handling.

0.1.0 [03/02/24]
* Added CMake support.
* Supports Visual Studio 22 (Windows 10) and
  g++ 13.2 and Clang 16.06 (Ubuntu 23.10).
* Run both event and machine handling on separate threads.
* Not using modules.
* Split SpaceInvaders.cpp/h into base controller and
  SDL controller components.
* Updated to run with mach-emu-1.3.0.
* Initial release.
