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

add_executable(${projectName}
    include/SpaceInvaders/IoController.h 
    include/SpaceInvaders/SdlIoController.h
    include/SpaceInvaders/MemoryController.h
    source/main.cpp
    source/IoController.cpp
    source/SdlIoController.cpp
    source/MemoryController.cpp
)

target_compile_definitions(${projectName} PRIVATE ROMS_DIR=\"${CMAKE_SOURCE_DIR}/roms/\")

find_library(libMachEmu MachEmu)
target_include_directories(${projectName} PRIVATE ${machEmuIncludePath} ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(${projectName} PRIVATE ${libMachEmu})

if(enableSdl)
    find_library(libSDL2 SDL2)
    find_library(libSDL2_mixer SDL2_mixer)
    target_include_directories(${projectName} PRIVATE ${sdlIncludePath})
    target_link_libraries(${projectName} PRIVATE ${libSDL2} ${libSDL2_mixer})
endif()
