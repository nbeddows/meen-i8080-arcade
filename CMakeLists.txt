# Copyright (c) 2021-2025 Nicolas Beddows <nicolas.beddows@gmail.com>

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
set(project_name meen-i8080-arcade)
set(major 1)
set(minor 1)
set(bugfix 0)

if(${enable_board} STREQUAL "pico")
  include ($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
endif()

project(${project_name} VERSION ${major}.${minor}.${bugfix} LANGUAGES C CXX ASM)
set(CMAKE_CXX_STANDARD 23)

if(${enable_board} STREQUAL "pico")
  pico_sdk_init()
endif()

if(DEFINED MSVC)
  set(build_type $<CONFIG>)
else()
  set(build_type ${CMAKE_BUILD_TYPE})
endif()

if(NOT DEFINED build_arch)
  set(build_arch ${CMAKE_SYSTEM_PROCESSOR})
endif()

if(NOT DEFINED build_os)
  set(build_os ${CMAKE_SYSTEM_NAME})
endif()

if(NOT DEFINED archive_dir)
  set(archive_dir "lib")
endif()

if(NOT DEFINED runtime_dir)
  set(runtime_dir "bin")
endif()

if(DEFINED WIN32)
  set(soExt .dll)
else()
  set(soExt .so)
endif()

# TODO: rp2040 does not output all artifacts correctly when setting this - investigate me
if(NOT ${enable_board} STREQUAL "pico")
  set(artifacts_dir $<1:${CMAKE_SOURCE_DIR}/artifacts/${build_type}/${build_arch}>)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${artifacts_dir}/${runtime_dir})
endif()

find_package(meen REQUIRED)
find_package(meen_hw REQUIRED)
find_package(ArduinoJson REQUIRED)

set(${project_name}_source_files
  source/GlyphRenderer.cpp
  source/main.cpp
  source/MemoryController.cpp
)

set(${project_name}_include_files
  include/meen_i8080_arcade/GlyphRenderer.h
  include/meen_i8080_arcade/IIoController.h
  include/meen_i8080_arcade/MemoryController.h
)

if(DEFINED MSVC)
  include(resource/generate_product_version.cmake)

  generate_product_version(
    versionResource
    COMPANY_NAME "Nicolas Beddows"
    FILE_DESCRIPTION "i8080 arcade emulator"
    NAME "MEEN i8080 Arcade"
    ORIGINAL_FILE_NAME "${project_name}.exe"
    PRODUCT_ICON ""
    VERSION_MAJOR ${major}
    VERSION_MINOR ${minor}
    VERSION_PATCH ${bugfix}
  )

  set (${project_name}_resource_files
    ${versionResource}
  )
endif()

set(${project_name}_deps meen::meen meen_hw::meen_hw ArduinoJson)

if(${build_os} STREQUAL "baremetal")
  if(${enable_board} STREQUAL "pico")
    set(${project_name}_include_files ${${project_name}_include_files} include/meen_i8080_arcade/RPIoController.h)
    set(${project_name}_source_files ${${project_name}_source_files} source/RPConfig.S source/RPIoController.cpp)
    set(${project_name}_deps ${${project_name}_deps} pico_stdlib hardware_dma hardware_spi hardware_gpio hardware_pio hardware_pwm)
  endif()
else()
  if(${enable_framework} STREQUAL "sdl")
    find_package(SDL2 REQUIRED)
    find_package(SDL2_mixer REQUIRED)
    set(${project_name}_include_files ${${project_name}_include_files} include/meen_i8080_arcade/SdlIoController.h)
    set(${project_name}_source_files ${${project_name}_source_files} source/SdlIoController.cpp)
    set(${project_name}_deps ${${project_name}_deps} SDL2::SDL2 SDL2_mixer::SDL2_mixer)
  endif()
endif()

add_executable(${project_name}
  ${${project_name}_include_files}
  ${${project_name}_source_files}
  ${${project_name}_resource_files}
)

if(DEFINED MSVC)
    set_target_properties(${project_name} PROPERTIES VS_DEBUGGER_COMMAND_ARGUMENTS "\"${CMAKE_SOURCE_DIR}/conf/config.json\"")
endif()

target_include_directories(${project_name} PRIVATE ${CMAKE_SOURCE_DIR}/include)

if(${build_os} STREQUAL "baremetal")
  # This is here for reference only. We don't do it this way since there is a conflict with the Conan and Pico
  # toolchain files which results in the wrong compiler being used to build pioasm.
  #pico_generate_pio_header(${project_name} ${CMAKE_SOURCE_DIR}/source/RPAudioOut.pio)

  # Use the prebuilt pioasm to generate the header (see the README for further details).
  add_executable(pio-asm IMPORTED)
  set_property(TARGET pio-asm PROPERTY IMPORTED_LOCATION $ENV{PICO_SDK_PATH}/build/pioasm/pioasm)
  add_custom_target(${project_name}-audio-pio-header COMMAND pio-asm RPAudioOut.pio RPAudioOut.pio.h WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/source COMMENT "Building Audio PIO Header")
  add_dependencies(${project_name} ${project_name}-audio-pio-header)

  target_include_directories(${project_name} PRIVATE ${CMAKE_SOURCE_DIR}/conf ${CMAKE_SOURCE_DIR}/rom-files ${CMAKE_SOURCE_DIR}/audio-files)
endif()

target_link_libraries(${project_name} PRIVATE ${${project_name}_deps})

# CPACK INSTALL
set(CMAKE_INSTALL_PREFIX ./)
set(CPACK_PACKAGE_FILE_NAME ${project_name}-v${CMAKE_PROJECT_VERSION}-${build_os}-${build_arch}-${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION})
set(CPACK_GENERATOR TGZ)

if(${build_os} STREQUAL "baremetal")
  if(${enable_framework} STREQUAL "st7789vw")
    target_compile_definitions(${project_name} PRIVATE ENABLE_MH_ST7789VW)
  endif()

  if(${enable_board} STREQUAL "pico")
    target_compile_definitions(${project_name} PRIVATE ENABLE_MH_RP2040)
    pico_enable_stdio_usb(${project_name} 1)
    pico_enable_stdio_uart(${project_name} 0)
    pico_set_program_url(${project_name} "https://github.com/nbeddows/${project_name}")
    pico_add_extra_outputs(${project_name})
    install(PROGRAMS ${CMAKE_BINARY_DIR}/${project_name}.bin DESTINATION ${runtime_dir})
    install(FILES ${CMAKE_BINARY_DIR}/${project_name}.uf2 DESTINATION ${runtime_dir})
    install(FILES ${CMAKE_BINARY_DIR}/${project_name}.dis DESTINATION ${archive_dir})
    install(FILES ${CMAKE_BINARY_DIR}/${project_name}.elf.map DESTINATION ${archive_dir})
    install(FILES ${CMAKE_BINARY_DIR}/${project_name}.hex DESTINATION ${archive_dir})
  endif()
else()
  if(${enable_framework} STREQUAL "sdl")
    target_compile_definitions(${project_name} PRIVATE SDL_MAIN_HANDLED)
  endif()

  if(DEFINED WIN32)
    set(scriptExt .bat)

    file(WRITE ${CMAKE_BINARY_DIR}/run-${project_name}${scriptExt}
    "@echo off\n\
set PATH=%cd%\\lib;%PATH%\n\
bin\\${project_name} %*\n\
pause\n\
exit"
    )
  else()
    set(scriptExt .sh)

    file(WRITE ${CMAKE_BINARY_DIR}/run-${project_name}${scriptExt}
    "#!/bin/bash\n\
export LD_LIBRARY_PATH=`pwd`/lib:\${LD_LIBRARY_PATH}\n\
bin/${project_name} \"$@\""
    )
  endif()

  install(PROGRAMS ${CMAKE_BINARY_DIR}/run-${project_name}${scriptExt} DESTINATION .)
endif()

if(DEFINED CMAKE_STRIP)
  add_custom_target(${project_name}-strip-pkg
    COMMAND ${CMAKE_COMMAND} -E tar -xzf ${CPACK_PACKAGE_FILE_NAME}.tar.gz
    COMMAND ${CMAKE_STRIP} -s ${CPACK_PACKAGE_FILE_NAME}/bin/${project_name}
    COMMAND ${CMAKE_STRIP} -s ${CPACK_PACKAGE_FILE_NAME}/lib/*.so
    COMMAND ${CMAKE_COMMAND} -E tar czf ${CPACK_PACKAGE_FILE_NAME}.tar.gz ${CPACK_PACKAGE_FILE_NAME}/
    COMMAND ${CMAKE_COMMAND} -E rm -r ${CPACK_PACKAGE_FILE_NAME}/
  )
endif()

install(CODE "execute_process(COMMAND doxygen ${CMAKE_SOURCE_DIR}/docs/Doxyfile)")
if(DEFINED MSVC)
  # Under Windows Doxygen documentaiton causes LaTeX to go into what looks to be an infinite loop, disable until a solution is found
  # install(CODE "execute_process(COMMAND cmd /c make WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/latex)")
# We are not going to build the documentation on arm platforms
elseif(${build_arch} STREQUAL x86_64)
  install(CODE "execute_process(COMMAND make WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/latex)")
endif()
install(TARGETS ${project_name} RUNTIME)
install(FILES CHANGELOG.md DESTINATION .)
install(FILES LICENSE.md DESTINATION . RENAME LICENSE)
install(DIRECTORY "conf" DESTINATION .)
install(DIRECTORY "audio-files" DESTINATION .)
install(DIRECTORY "rom-files" DESTINATION .)
# Latex to PDF generation under Windows is currently not working, remove the if protection once fixed
if(NOT DEFINED MSVC)
  if(${build_arch} STREQUAL x86_64)
    install(FILES ${CMAKE_BINARY_DIR}/latex/refman.pdf RENAME MEEN_i8080_Arcade_Reference.pdf DESTINATION .)
  endif()
endif()

if(DEFINED meenBinDir)
    install(DIRECTORY ${meenBinDir}/ DESTINATION lib FILES_MATCHING PATTERN "*${soExt}*")
endif()

if(DEFINED meenHwBinDir)
    install(DIRECTORY ${meenHwBinDir}/ DESTINATION lib FILES_MATCHING PATTERN "*${soExt}*")
endif()

if(DEFINED sdlBinDir)
    install(DIRECTORY ${sdlBinDir}/ DESTINATION lib FILES_MATCHING PATTERN "*${soExt}*")
endif()

if(DEFINED sdlMixerBinDir)
    install(DIRECTORY ${sdlMixerBinDir}/ DESTINATION lib FILES_MATCHING PATTERN "*${soExt}*")
endif()

if(DEFINED zlibBinDir)
    install(DIRECTORY ${zlibBinDir}/ DESTINATION lib FILES_MATCHING PATTERN "*${soExt}*")
endif()

if(NOT DEFINED WIN32)
    set(CPACK_SET_DESTDIR ON)
endif()

include(CPack)
