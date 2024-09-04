0.6.1 [04/08/24]
* Added profiles for improved build support.
* Compiler id and version are now incorporated
  into the package name.
* Removed target `i8080-arcade-pkg`.
* Now using the built in `package` target to
  build the package and perform static analysis.
* Added target `i8080-arcade-strip-pkg` for
  optional package stripping.
* Updated to meen_hw/0.2.1.

0.6.0 [25/07/24]
* Using meen-hw for i8080-arcade support.
* Added support for the following roms: `lunar-rescue`,
  `space-invaders-deluxe`, `space-invaders-ii` and
  `balloon-bomber`.
* Added command line option `-g` for rom selection.
* Added build target `i8080-arcade-pkg` which builds the
  package, strips the binaries and performs static
  analysis.
* Updated to mach-emu-1.6.2.
* Renamed repository to i8080-arcade.
* Replaced all occurrences of space-invaders with
  i8080-arcade.

0.5.0 [24/06/24]
* Updated to mach-emu-1.6.1.
* Using Conan package manager.
* Added CPack support for binary distributions.
* Updated the JSON config file for improved flexibility.

0.4.0 [1/05/24]
* Updated to mach-emu-1.5.1.
* Added `IMachine::Load` support.
* Added `IMachine::Save` support.
* Added support for Space Invaders Part II/Deluxe.
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
