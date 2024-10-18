# CMakeList.txt : CMake project for ToobAmp, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.18)

set(CMAKE_MODULE_PATH ${PROJECT_BINARY_DIR}/modules/lv2cairo)
#find_package(Lv2c REQUIRED)


set (CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (DEFINED BUILD_MACHINE)
    message(STATUS "Google Profiler support disabled.}")
    set (PROFILER 0) #no google-perf install on build machine.
else()
    # set(PROFILER 1)  # enables google profiler in ConvolutionReverbTest, ProfileNeuralAmpModeler. Requires google-perftools-dev
    set(PROFILER 0)  # disables google profiler in ConvolutionReverbTest, ProfileNeuralAmpModeler
endif()

execute_process(COMMAND
  dpkg-architecture
    -qDEB_HOST_ARCH
  OUTPUT_VARIABLE
    CMAKE_DEB_HOST_ARCH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)


if (CMAKE_BUILD_TYPE STREQUAL "Debug")
   add_compile_definitions("DEBUG")
else()
    add_compile_definitions("NDEBUG")
endif()


####################################################


# Tests.
add_executable(ToobNamProfiling 
  main.cpp ss.hpp
  util.cpp util.hpp
)

