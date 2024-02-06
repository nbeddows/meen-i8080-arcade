0.1.1 [07/07/24]
* Added a frame pool to prevent superfluous memory allocs.
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
