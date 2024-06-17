# Copyright (c) 2021-2024 Nicolas Beddows <nicolas.beddows@gmail.com>

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

cmake_minimum_required (VERSION 3.25)
set(projectName space-invaders)

project(${projectName} VERSION 0.5.0)

if(DEFINED MSVC)
  set(buildType $<CONFIG>)
else()
  set(buildType ${CMAKE_BUILD_TYPE})
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_SOURCE_DIR}/artifacts/${buildType}/${CMAKE_SYSTEM_PROCESSOR}/bin>)
set(configDir "${CMAKE_SOURCE_DIR}/conf" CACHE PATH "Path to the configuration file directory")
set(romsDir "${CMAKE_SOURCE_DIR}/roms" CACHE PATH "Path to the roms directory")

find_package(mach_emu REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(SDL2 REQUIRED)
find_package(SDL2_mixer REQUIRED)

add_executable(${projectName}
    include/SpaceInvaders/IoController.h 
    include/SpaceInvaders/SdlIoController.h
    include/SpaceInvaders/MemoryController.h
    source/main.cpp
    source/IoController.cpp
    source/SdlIoController.cpp
    source/MemoryController.cpp
)

target_include_directories(${projectName} PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(${projectName} PRIVATE CONFIG_DIR=\"${configDir}\" ROMS_DIR=\"${romsDir}\")
target_link_libraries(${projectName} PRIVATE
    mach_emu::mach_emu
    nlohmann_json::nlohmann_json
    SDL2::SDL2
    SDL2_mixer::SDL2_mixer
)

target_compile_definitions(${projectName} PRIVATE SDL_MAIN_HANDLED)
