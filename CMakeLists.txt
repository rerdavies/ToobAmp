
# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required(VERSION 3.10)
set(VERBOSE true)
set(CMAKE_VERBOSE_MAKEFILE ON)

project(ToobAmp VERSION 1.2.77 DESCRIPTION "TooB LV2 Guitar Effects Plugins")
# Semantic Version release type. e.g. "-alpha", "-beta3" or "" for a release.
set(PROJECT_RELEASE_QUALIFIER "")


set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


option(TOOBAMP_NATIVE_AARCH_OPTIMIZATION "Optimize for native AARCH" OFF)
option(TOOBAMP_A76_OPTIMIZATION "Optimize for cortex-a76 (arm64 builds only)" OFF)
option(TOOBAMP_NATIVE_X64_OPTIMIZATIONS "Optimize native processor options (x86_64 builds only)" ON)
option(TOOBAMP_AVX_X64_OPTIMIZATIONS "Optimize for processor with AVXprocessor options (x86_64 builds only)" OFF)
option(TOOBAMP_PERMISSIVE_COMPILE "Enable permissive compile options (warnings are not errors)" OFF)

option(TOOB_BUILD_UI "Build the X11 UI" ON) # Build the UI Plugin..
option(WARNINGS_ARE_ERRORS "Treat warnings as errors." ON)

option(TOOB_MULTI_ARCH_BUILD "Install both a72 and a76 Toob plugins in the same package." OFF)
option(INSTALL_LV2CAIRO_TEST_PLUGIN "Install the Lv2Cairo Test Plugin" OFF) # Install the lv2cairo test plugin.


add_compile_definitions(NAM_SAMPLE_FLOAT=1)

add_compile_definitions(NAM_ENABLE_A2_FAST)

# NueralAmpModelerCore has a more recent version of Eigen than NeuralAudio does, so 
# force usage of the NeuralAmpModelerCore version of Eigen. 
include_directories("${PROJECT_SOURCE_DIR}/modules/NeuralAmpModelerCore/Dependencies/eigen")


set(TOOBAMP_SEMANTIC_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}${PROJECT_RELEASE_QUALIFIER}")

message(STATUS "ToobAmp Version: ${TOOBAMP_SEMANTIC_VERSION}")

execute_process(COMMAND
    dpkg-architecture
    -qDEB_HOST_ARCH
    OUTPUT_VARIABLE
    CMAKE_DEB_HOST_ARCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

message(STATUS "CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}")

add_compile_options("-DDISABLE_DENORMALS")

set(USE_NEURAL_MODELER_NAM_CORE ON)
set(TOOBAMP_COMPILE_OPTIONS "")

# Force architecture for all submodule builds.
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    #RelWithDebugInfo is -Oe
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO TYPE STRING FORCE VALUE -O3 -Ofast -g -DNDEBUG)

    # Match NeuralAudio compile options
    set(TOOBAMP_COMPILE_OPTIONS "-flto=auto -fno-fat-lto-objects -O3 -Ofast")


    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
        set(CMAKE_LINKER_FLAG)
        if(TOOBAMP_NATIVE_AARCH_OPTIMIZATION)
            add_compile_options("-mcpu=native" "-march=native" "-mtune=native")
        elseif(TOOBAMP_A76_OPTIMIZATION)
            message(STATUS "Using Cortex-A76/Pi 5 optimizations")
            add_compile_options("-DA76_OPTIMIZATION=1" "-mcpu=cortex-a76" "-march=armv8.2-a" "-mtune=cortex-a76")
        else()
            message(STATUS "Using Cortex-A72/Pi 4 optimizations")
            add_compile_options("-mcpu=cortex-a72" "-mtune=cortex-a72")
        endif()
    elseif((CMAKE_SYSTEM_PROCESSOR MATCHES "amd64") OR (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64") OR (CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64"))

        if(TOOBAMP_NATIVE_X64_OPTIMIZATIONS)
            message(STATUS "Using Native x86-64 optimizations")
            add_compile_options("-mcpu=native" "-march=native" "-mtune=native")
        elseif(TOOBAMP_AVX_X64_OPTIMIZATIONS)
            message(STATUS "Using AVX x86-64 optimizations")
            add_compile_options(-march=x86-64-v3)
        else()
            message(STATUS "Not using AVX x86-64 optimizations")
            add_compile_options(-march=x86-64-v2)
        endif()
    endif()
endif()


message(STATUS "TOOBAMP_COMPILE_OPTIONS: ${TOOBAMP_COMPILE_OPTIONS}")

include(CTest)
enable_testing()

set(CMAKE_CXX_STANDARD 20)

if(WIN32)
    include_directories("C:/Users/rerda/source/lv2-1.18.2")
else()
    include_directories("/usr/local/include")
endif()



# Include submodules
add_subdirectory("modules")

# debian copyright utils.
add_subdirectory("debian/src")

add_subdirectory("ToobNamProfiling")

set(CMAKE_CXX_STANDARD 20 CACHE STRING "Default C++ standard")
# Include sub-projects.
add_subdirectory("src")

# -- this code has been replaced by redavies/pipedal's profilePlugin, which provides a full LV2 Host implementation.
# add_subdirectory("Test") - not currently working.


set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Robin Davies <rerdavies at gmail.com>") # required
set(CPACK_PACKAGE_VENDOR "Robin Davies")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/debian/package_description.txt")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "TooB LV2 Guitar Effects Pedals")
set(CPACK_DEBIAN_PACKAGE_SECTION sound)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION TRUE)
#set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "arm64")
set(CPACK_PACKAGING_INSTALL_PREFIX /usr/local)
set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_STRIP_FILES YES)

# set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/debian/postinst;${CMAKE_CURRENT_SOURCE_DIR}/debian/prerm")


include(CPack)
