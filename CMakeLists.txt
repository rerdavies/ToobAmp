# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.9)

set(CMAKE_VERBOSE_MAKEFILE ON CACHE BOOL "ON")

project(ToobAmp VERSION 0.0.2 DESCRIPTION "TooB Amp LV2 Plugin")

if(WIN32)
	include_directories("C:/Users/rerda/source/lv2-1.18.2")
	add_compile_options("/std:c++17")
else()
	include_directories("/usr/local/include")
endif()

if (MSVC)
else()
    # lots of warnings and all warnings as errors
    add_compile_options("-std=c++17")
endif()


# Include sub-projects.
add_subdirectory ("src")
if (WIN32)
#	add_subdirectory ("Test")
endif()