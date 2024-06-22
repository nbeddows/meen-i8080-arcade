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

if(DEFINED WIN32)
  set(ext .exe)
endif()

set(artifactsDir $<1:${CMAKE_SOURCE_DIR}/artifacts/${buildType}/${CMAKE_SYSTEM_PROCESSOR}>)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${artifactsDir}/bin)

find_package(mach_emu REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(popl REQUIRED)
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

if(DEFINED MSVC)
    set_target_properties(${projectName} PROPERTIES VS_DEBUGGER_COMMAND_ARGUMENTS "\"--config-file=${CMAKE_SOURCE_DIR}/conf/config.json\" \"--rom-file-path=${CMAKE_SOURCE_DIR}/rom-files\" \"--audio-file-path=${CMAKE_SOURCE_DIR}/audio-files\" \"--save-file-path=${CMAKE_SOURCE_DIR}/save-files\"")
endif()

target_include_directories(${projectName} PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(${projectName} PRIVATE
    mach_emu::mach_emu
    nlohmann_json::nlohmann_json
    popl::popl
    SDL2::SDL2
    SDL2_mixer::SDL2_mixer
)

target_compile_definitions(${projectName} PRIVATE SDL_MAIN_HANDLED)

install(FILES ${artifactsDir}/bin/${projectName}${ext} DESTINATION /)
install(FILES LICENSE.md DESTINATION ./ RENAME LICENSE)
install(FILES README.md DESTINATION ./)
install(DIRECTORY "conf" DESTINATION ./)
install(DIRECTORY "audio-files" DESTINATION ./)
install(DIRECTORY "rom-files" DESTINATION ./)

if(DEFINED machEmuBinDir)
    install(DIRECTORY ${machEmuBinDir}/ DESTINATION ./ FILES_MATCHING PATTERN "*")
endif()

if(DEFINED sdlBinDir)
    install(DIRECTORY ${sdlBinDir}/ DESTINATION ./ FILES_MATCHING PATTERN "*")
endif()

if(DEFINED sdlMixerBinDir)
    install(DIRECTORY ${sdlMixerBinDir}/ DESTINATION ./ FILES_MATCHING PATTERN "*")
endif()

if(DEFINED zlibBinDir)
    install(DIRECTORY ${zlibBinDir}/ DESTINATION ./ FILES_MATCHING PATTERN "*")
endif()

if(NOT DEFINED WIN32)
    set(CPACK_SET_DESTDIR ON)
endif()

include(CPack)
