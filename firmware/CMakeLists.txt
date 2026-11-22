cmake_minimum_required(VERSION 3.25)

project(excimer_burst_firmware
  VERSION 2.0.0
  DESCRIPTION "Bare-metal pulse-energy controller (ILC + PID) for excimer bursts"
  LANGUAGES C CXX)

set(EBC_PLATFORM "host" CACHE STRING "host | zynqmp_a53 | zynqmp_r5f | cortex_m7")
set_property(CACHE EBC_PLATFORM PROPERTY STRINGS host zynqmp_a53 zynqmp_r5f cortex_m7)
set(EBC_CXX_STANDARD 23 CACHE STRING "ISO C++ standard: 23, or 26 to track the next one")
option(EBC_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

set(CMAKE_CXX_STANDARD ${EBC_CXX_STANDARD})
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(EbcWarnings)
include(EbcFirmware)

add_subdirectory(core)
add_subdirectory(app)

if(EBC_PLATFORM STREQUAL "host")
  include(CTest)
  add_subdirectory(bsp/host)
  if(BUILD_TESTING)
    add_subdirectory(tests)
  endif()
else()
  enable_language(ASM)
  add_subdirectory(bsp/${EBC_PLATFORM})
endif()

message(STATUS "EBC: platform=${EBC_PLATFORM} C++${CMAKE_CXX_STANDARD} C${CMAKE_C_STANDARD} "
               "compiler=${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
