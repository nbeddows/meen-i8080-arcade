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
set(project_name i8080-arcade)

if(${enable_rp2040} STREQUAL ON)
  include ($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
endif()

project(${project_name} VERSION 0.6.1 LANGUAGES C CXX ASM)

if(${enable_rp2040} STREQUAL ON)
  add_executable(Pioasm IMPORTED)
  set_property(TARGET Pioasm PROPERTY IMPORTED_LOCATION $ENV{PICO_SDK_PATH}/build/pioasm/pioasm)
  set(Pioasm_FOUND 1)

  # In the 2.x SDK this is handled by picotool, need to account for this with version numbers
  add_executable(ELF2UF2 IMPORTED)
  set_property(TARGET ELF2UF2 PROPERTY IMPORTED_LOCATION $ENV{PICO_SDK_PATH}/build/elf2uf2/elf2uf2)
  set(ELF2UF2_FOUND 1)

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
if(NOT ${enable_rp2040} STREQUAL ON)
  set(artifacts_dir $<1:${CMAKE_SOURCE_DIR}/artifacts/${build_type}/${build_arch}>)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${artifacts_dir}/${runtime_dir})
endif()

find_package(mach_emu REQUIRED)
find_package(meen_hw REQUIRED)
find_package(ArduinoJson REQUIRED)

set(${project_name}_source_files source/main.cpp source/MemoryController.cpp)
set(${project_name}_include_files include/i8080_arcade/MemoryController.h)
set(${project_name}_deps mach_emu::mach_emu meen_hw::meen_hw ArduinoJson)

if(${build_os} STREQUAL "baremetal")
  if(${enable_rp2040} STREQUAL ON)
    set(${project_name}_include_files ${${project_name}_include_files} include/i8080_arcade/RPIoController.h)
    set(${project_name}_source_files ${${project_name}_source_files} source/RPConfig.S source/RPIoController.cpp)
    set(${project_name}_deps ${${project_name}_deps} pico_stdlib)
  endif()
else()
  find_package(popl REQUIRED)
  set(${project_name}_deps ${${project_name}_deps} popl::popl)

  if(${enable_sdl} STREQUAL ON)
    find_package(SDL2 REQUIRED)
    find_package(SDL2_mixer REQUIRED)
    set(${project_name}_include_files ${${project_name}_include_files} include/i8080_arcade/SdlIoController.h)
    set(${project_name}_source_files ${${project_name}_source_files} source/SdlIoController.cpp)
    set(${project_name}_deps ${${project_name}_deps} SDL2::SDL2 SDL2_mixer::SDL2_mixer)
  endif()
endif()

add_executable(${project_name}
  ${${project_name}_include_files}
  ${${project_name}_source_files}
)

if(DEFINED MSVC)
    set_target_properties(${project_name} PROPERTIES VS_DEBUGGER_COMMAND_ARGUMENTS "\"--config-file=${CMAKE_SOURCE_DIR}/conf/config.json\" \"--rom-file-path=${CMAKE_SOURCE_DIR}/rom-files\" \"--audio-file-path=${CMAKE_SOURCE_DIR}/audio-files\" \"--save-file-path=${CMAKE_SOURCE_DIR}/save-files\"")
endif()

target_include_directories(${project_name} PRIVATE ${CMAKE_SOURCE_DIR}/include)

if(${build_os} STREQUAL "baremetal")
  target_include_directories(${project_name} PRIVATE ${CMAKE_SOURCE_DIR}/conf ${CMAKE_SOURCE_DIR}/rom-files)
endif()

target_link_libraries(${project_name} PRIVATE ${${project_name}_deps})

# CPACK INSTALL
set(CMAKE_INSTALL_PREFIX ./)
set(CPACK_PACKAGE_FILE_NAME ${project_name}-v${CMAKE_PROJECT_VERSION}-${build_os}-${build_arch}-${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION})
set(CPACK_GENERATOR TGZ)

if(${build_os} STREQUAL "baremetal")
  if(${enable_st7789vw} STREQUAL ON)
    target_compile_definitions(${project_name} PRIVATE ENABLE_MH_ST7789VW)
  endif()

  if(${enable_rp2040} STREQUAL ON)
    target_compile_definitions(${project_name} PRIVATE ENABLE_MH_RP2040)
    pico_enable_stdio_usb(${project_name} 1)
    pico_enable_stdio_uart(${project_name} 0)
    pico_set_program_url(${project_name} "https://github.com/neddows/meen-${project_name}")
    pico_add_extra_outputs(${project_name})
    install(PROGRAMS ${CMAKE_BINARY_DIR}/tests/${project_name}/${project_name}.bin DESTINATION ${runtime_dir})
    install(FILES ${CMAKE_BINARY_DIR}/tests/${project_name}/${projcet_name}.uf2 DESTINATION ${runtime_dir})
    install(FILES ${CMAKE_BINARY_DIR}/tests/${project_name}/${project_name}.dis DESTINATION ${archive_dir})
    install(FILES ${CMAKE_BINARY_DIR}/tests/${project_name}/${project_name}.elf.map DESTINATION ${archive_dir})
    install(FILES ${CMAKE_BINARY_DIR}/tests/${project_name}/${project_name}.hex DESTINATION ${archive_dir})
  endif()
else()
  if(${enable_sdl} STREQUAL ON)
    #target_compile_definitions(${project_name} PRIVATE ENABLE_MH_SDL2)
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

#install(CODE "execute_process(COMMAND doxygen ${CMAKE_SOURCE_DIR}/docs/Doxyfile)")
install(CODE "execute_process(COMMAND cppcheck -ibuild -i_CPack_Packages -itests --check-level=exhaustive --inline-suppr --enable=warning,style,performance,portability,unusedFunction --std=c++20 ${CMAKE_SOURCE_DIR})")
install(TARGETS ${project_name} RUNTIME)
install(FILES LICENSE.md DESTINATION . RENAME LICENSE)
install(FILES docs/README-bin.md DESTINATION . RENAME README.md)
install(DIRECTORY docs/images DESTINATION ./docs)
install(DIRECTORY "conf" DESTINATION .)
install(DIRECTORY "audio-files" DESTINATION .)
install(DIRECTORY "rom-files" DESTINATION .)

if(DEFINED machEmuBinDir)
    install(DIRECTORY ${machEmuBinDir}/ DESTINATION lib FILES_MATCHING PATTERN "*${soExt}*")
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
