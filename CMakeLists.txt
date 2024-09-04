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

project(${project_name} VERSION 0.6.1)

if(DEFINED MSVC)
  set(build_type $<CONFIG>)
else()
  set(build_type ${CMAKE_BUILD_TYPE})
endif()

set(artifacts_dir $<1:${CMAKE_SOURCE_DIR}/artifacts/${build_type}/${CMAKE_SYSTEM_PROCESSOR}>)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${artifacts_dir}/bin)

find_package(mach_emu REQUIRED)
find_package(meen_hw REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(popl REQUIRED)
find_package(SDL2 REQUIRED)
find_package(SDL2_mixer REQUIRED)

add_executable(${project_name}
    include/i8080_arcade/SdlIoController.h
    include/i8080_arcade/MemoryController.h
    source/main.cpp
    source/SdlIoController.cpp
    source/MemoryController.cpp
)

if(DEFINED MSVC)
    set_target_properties(${project_name} PROPERTIES VS_DEBUGGER_COMMAND_ARGUMENTS "\"--config-file=${CMAKE_SOURCE_DIR}/conf/config.json\" \"--rom-file-path=${CMAKE_SOURCE_DIR}/rom-files\" \"--audio-file-path=${CMAKE_SOURCE_DIR}/audio-files\" \"--save-file-path=${CMAKE_SOURCE_DIR}/save-files\"")
endif()

target_include_directories(${project_name} PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(${project_name} PRIVATE
    mach_emu::mach_emu
    meen_hw::meen_hw
    nlohmann_json::nlohmann_json
    popl::popl
    SDL2::SDL2
    SDL2_mixer::SDL2_mixer
)

target_compile_definitions(${project_name} PRIVATE SDL_MAIN_HANDLED)

# CPACK INSTALL
set(CMAKE_INSTALL_PREFIX ./)
set(CPACK_PACKAGE_FILE_NAME ${project_name}-v${CMAKE_PROJECT_VERSION}-${CMAKE_SYSTEM}-${CMAKE_SYSTEM_PROCESSOR}-${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION})
set(CPACK_GENERATOR TGZ)

if(DEFINED WIN32)
  set(soExt .dll)
  set(scriptExt .bat)

  file(WRITE ${CMAKE_BINARY_DIR}/run-${project_name}${scriptExt}
  "@echo off\n\
set PATH=%cd%\\lib;%PATH%\n\
bin\\${project_name} %*\n\
pause\n\
exit"
)
else()
  set(soExt .so)
  set(scriptExt .sh)

  file(WRITE ${CMAKE_BINARY_DIR}/run-${project_name}${scriptExt}
  "#!/bin/bash\n\
export LD_LIBRARY_PATH=`pwd`/lib:\${LD_LIBRARY_PATH}\n\
bin/${project_name} \"$@\""
)  
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
install(PROGRAMS ${CMAKE_BINARY_DIR}/run-${project_name}${scriptExt} DESTINATION .)
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
