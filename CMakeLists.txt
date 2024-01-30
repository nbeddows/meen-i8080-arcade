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

cmake_minimum_required (VERSION 3.28)
#set(CMAKE_CXX_COMPILER g++-13)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/$<CONFIG>)
set(projectName space-invaders)
option(enableSdl "Use the SDL IO controller" ON)

if(WIN32)
    set(machEmuIncludePath "C://Program Files/mach-emu/include" CACHE PATH "Path to the mach-emu includes")

    if(enableSdl)
        set(sdlIncludePath "C://Program Files/SDL2/include" CACHE PATH "Path to the SDL includes")
    endif()
else()
    set(machEmuIncludePath "/usr/local/include/" CACHE PATH "Path to the mach-emu includes")

    if(enableSdl)
        set(sdlIncludePath "/usr/include" CACHE PATH "Path to the SDL includes")
    endif()
endif()

project(${projectName})

if(CMAKE_COMPILER_IS_GNUCXX)
    set(STD_MODULES array bitset memory)

    set(STD_MODULES_BUILD_TIMESTAMP_FILE std_modules_build_timestamp)
    # One time build only
    add_custom_target(StdModules ALL
        DEPENDS ${STD_MODULES_BUILD_TIMESTAMP_FILE}
    )

    set(STD_MODULES_OPTIONS -fmodules-ts -std=c++20 -c -x c++-system-header)

    # compile the required std modules
    add_custom_command(
        OUTPUT ${STD_MODULES_BUILD_TIMESTAMP_FILE}
        COMMAND ${CMAKE_COMMAND} -E echo "Building standard libraries modules"
        COMMAND ${CMAKE_CXX_COMPILER} ${STD_MODULES_OPTIONS} ${STD_MODULES}
        COMMAND ${CMAKE_COMMAND} -E touch ${STD_MODULES_BUILD_TIMESTAMP_FILE}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        BYPRODUCTS
    )
endif()

add_executable(${projectName} module/SpaceInvaders.ixx source/main.cpp source/SpaceInvaders.cpp)
target_compile_definitions(${projectName} PRIVATE ROMS_DIR=\"${CMAKE_SOURCE_DIR}/roms/\")

if(CMAKE_COMPILER_IS_GNUCXX)
    target_compile_options(${projectName} PRIVATE -x c++ -fmodules-ts)
endif()

find_library(libMachEmu MachEmu)
target_include_directories(${projectName} PRIVATE ${machEmuIncludePath})
target_link_libraries(${projectName} PRIVATE ${libMachEmu})

if(enableSdl)
    find_library(libSDL2 SDL2)
    find_library(libSDL2_mixer SDL2_mixer)
    target_include_directories(${projectName} PRIVATE ${sdlIncludePath})
    target_link_libraries(${projectName} PRIVATE ${libSDL2} ${libSDL2_mixer})
endif()
