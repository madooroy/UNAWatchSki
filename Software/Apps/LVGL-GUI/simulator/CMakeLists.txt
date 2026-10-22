# Ski Day LVGL PC simulator: the app's real service and LVGL GUI processes
# running against the SDK's mock kernel, with the display in an SDL2 window
# and the buzzer cues played through the PC speakers. Start a demo day from
# Settings to play a whole ski day through the app.
#
# Linux / WSL (Debian/Ubuntu: build-essential cmake ninja-build libsdl2-dev):
#   export UNA_SDK=/path/to/una-sdk
#   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
#   cmake --build build
#   (cd build/bin && ./SkiDaySimulator)
# Windows: see the SDK's RunLVGL tutorial for the MSVC equivalents.
#
# Keys: 1 = L1, 2 = L2, 3 = R1, 4 = R2, Esc = quit. UNA_SIM_MUTE=1 silences it.
cmake_minimum_required(VERSION 3.21)

if("$ENV{UNA_SDK}" STREQUAL "")
    message(FATAL_ERROR "UNA_SDK environment variable is not set. Point it at the SDK root.")
endif()

project(SkiDaySimulator C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

include($ENV{UNA_SDK}/cmake/una-simulator.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../../Libs/libs.cmake)   # LIBS_SOURCES, LIBS_INCLUDE_DIRS
include(${CMAKE_CURRENT_SOURCE_DIR}/../lvgl-gui.cmake)          # GUI_APP_SOURCES, GUI_APP_INCLUDE_DIRS

# Run from build/bin so the mock file system lands in the repo's Output/.
set(SIM_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${SIM_BIN_DIR}")
foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER ${cfg} CFG)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG} "${SIM_BIN_DIR}")
endforeach()

add_executable(SkiDaySimulator
    main.cpp
    AudioBuzzer.cpp
    ${GUI_APP_SOURCES}
    ${LIBS_SOURCES}
    ${UNA_SDK_SOURCES_SIMULATOR}
    ${UNA_SDK_SOURCES_SIMULATOR_SERVICE}
    ${UNA_SDK_SOURCES_SIMULATOR_LVGL}
)

target_include_directories(SkiDaySimulator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GUI_APP_INCLUDE_DIRS}
    ${LIBS_INCLUDE_DIRS}
    ${UNA_SDK_INCLUDE_DIRS_SIMULATOR}
    ${UNA_SDK_INCLUDE_DIRS_GUI_LVGL}
)

target_compile_definitions(SkiDaySimulator PRIVATE
    ${UNA_SDK_DEFINES_SIMULATOR}
    ${UNA_SDK_DEFINES_GUI_LVGL}
    "LV_MEM_SIZE=(96U*1024U)"   # as in SkiDay-CMake/CMakeLists.txt
    APP_ID="CDCE61DE7F70982C"
    DEV_ID="UNA Simulator"
    BUILD_VERSION="1.0.0-sim"
)

find_package(Threads REQUIRED)
target_link_libraries(SkiDaySimulator PRIVATE Threads::Threads)
una_simulator_link_sdl2(SkiDaySimulator)

if(MSVC)
    target_compile_options(SkiDaySimulator PRIVATE /W3 /MP)
else()
    target_compile_options(SkiDaySimulator PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
