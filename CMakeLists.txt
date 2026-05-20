
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
set(TOOB_AMD_OPTIMIZATIONS "AVX" CACHE STRING "AMD/x86 optimization profile: DEFAULT, SSE42, AVX, AVX-512")

# TOOB_AMD_OPTIMIZATIONS: 
#  "DEFAULT" All x64-capable machines.
#  "SSE42" for maximum compatibility. Intel since 2009; AMD since 2012.
#   "AVX" for best performance on modern AMD CPUs. Intel since 2015, AMD and Atom processors since 2017.
#   "AVX-512" is not currently recommended, as it often performs less well than AVX.
set(TOOB_AMD_OPTIMIZATIONS "AVX" CACHE STRING "AMD/x86 optimization profile: DEFAULT, SSE42, AVX, AVX-512")
set_property(CACHE TOOB_AMD_OPTIMIZATIONS PROPERTY STRINGS "DEFAULT" "SSE42" "AVX" "AVX-512")
set(_TOOB_AMD_OPT_VALUES "DEFAULT" "SSE42" "AVX" "AVX-512") 
list(FIND _TOOB_AMD_OPT_VALUES "${TOOB_AMD_OPTIMIZATIONS}" _TOOB_AMD_OPT_INDEX)
if(_TOOB_AMD_OPT_INDEX EQUAL -1)
    message(FATAL_ERROR "Invalid TOOB_AMD_OPTIMIZATIONS='${TOOB_AMD_OPTIMIZATIONS}'. Allowed values: DEFAULT, SSE42, AVX, AVX-512")
endif()
unset(_TOOB_AMD_OPT_VALUES)
unset(_TOOB_AMD_OPT_INDEX)


# TOOB_AARCH_OPTIMIZATIONS:
#   "NATIVE" - optimize for build machine
#   "A72" - optimize for Pi 4.
#   "A76" - optimize for Pi 5.
#   "DEFAULT" - Default compiler options.

set(TOOB_AARCH_OPTIMIZATIONS "A72" CACHE STRING "AARCH optimization profile: A72, A76, NATIVE, DEFAULT")
set_property(CACHE TOOB_AARCH_OPTIMIZATIONS PROPERTY STRINGS "A72" "A76" "NATIVE" "DEFAULT")
set(_TOOB_AARCH_OPT_VALUES "A72" "A76" "NATIVE" "DEFAULT")
list(FIND _TOOB_AARCH_OPT_VALUES "${TOOB_AARCH_OPTIMIZATIONS}" _TOOB_AARCH_OPT_INDEX)
if(_TOOB_AARCH_OPT_INDEX EQUAL -1)
    message(FATAL_ERROR "Invalid TOOB_AARCH_OPTIMIZATIONS='${TOOB_AARCH_OPTIMIZATIONS}'. Allowed values: A72, A76, NATIVE, DEFAULT")
endif()
unset(_TOOB_AARCH_OPT_VALUES)
unset(_TOOB_AARCH_OPT_INDEX)




option(TOOBAMP_PERMISSIVE_COMPILE "Enable permissive compile options (warnings are not errors)" OFF)

option(TOOB_BUILD_UI "Build the X11 UI" ON) # Build the UI Plugin..
option(TOOBAMP_WARNINGS_ARE_ERRORS "Treat warnings as errors." ON)

option(TOOB_MULTI_ARCH_BUILD "Install both a72 and a76 Toob plugins in the same package." OFF)
option(INSTALL_LV2CAIRO_TEST_PLUGIN "Install the Lv2Cairo Test Plugin" OFF) # Install the lv2cairo test plugin.


add_compile_definitions(NAM_SAMPLE_FLOAT=1)

set_property(CACHE TOOB_AMD_OPTIMIZATIONS PROPERTY STRINGS "DEFAULT" "SSE42" "AVX" "AVX-512")

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
if (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    add_compile_options("-Wno-nan-infinity-disabled")        
endif()

set(USE_NEURAL_MODELER_NAM_CORE ON)
set(TOOBAMP_COMPILE_OPTIONS "")

# Force all submodules to use the same optimization settings. 

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options("-O0" "-ggdb" "-DNDEBUG")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options("-O3" "-Ofast" "-DNDEBUG")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    add_compile_options("-ggdb" "-O3" "-Ofast" "-DNDEBUG")
endif()

# Force architecture for all submodule builds.

# Match NeuralAudio compile options


if (CMAKE_C_COMPILER_ID STREQUAL "GNU")

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
        set(CMAKE_LINKER_FLAG)
        if(TOOB_AARCH_OPTIMIZATIONS STREQUAL "NATIVE")
            add_compile_options("-mcpu=native" "-march=native" "-mtune=native")
            add_compile_definitions("TOOB_OPTIMIZATION_FLAGS=\"aarch64 (native)\"")
        elseif(TOOB_AARCH_OPTIMIZATIONS STREQUAL "A76")
            message(STATUS "Using Cortex-A76/Pi 5 optimizations")
            add_compile_options("-DA76_OPTIMIZATION=1" "-mcpu=cortex-a76" "-march=armv8.2-a" "-mtune=cortex-a76")
            add_compile_definitions("TOOB_OPTIMIZATION_FLAGS=\"cortex-a76\"")

            add_compile_definitions(
                "EIGEN_DEFAULT_L1_CACHE_SIZE=65536"
                "EIGEN_DEFAULT_L2_CACHE_SIZE=524288"
                "EIGEN_DEFAULT_L3_CACHE_SIZE=2097152"
            )

        elseif(TOOB_AARCH_OPTIMIZATIONS STREQUAL "A72")
            message(STATUS "Using Cortex-A72/Pi 4 optimizations")

            add_compile_options("-march=armv8-a" "-mtune=cortex-a72")
            add_compile_definitions("TOOB_OPTIMIZATION_FLAGS=\"armv8-a\"")

            add_compile_definitions(
                "EIGEN_DEFAULT_L1_CACHE_SIZE=32768"
                "EIGEN_DEFAULT_L2_CACHE_SIZE=1048576"
                "EIGEN_DEFAULT_L3_CACHE_SIZE=1048576"
            )

            # 37%
            # add_compile_options("-mcpu=cortex-a72" "-mtune=cortex-a72")
            # add_compile_definitions(TOOB_OPTIMIZATION_FLAGS=\"cortex-a72\")
        elseif(TOOB_AARCH_OPTIMIZATIONS STREQUAL "DEFAULT")
            message(STATUS "Using default AARCH optimizations")
            add_compile_definitions("TOOB_OPTIMIZATION_FLAGS=\"aarch64 default\"")
        endif()
    elseif((CMAKE_SYSTEM_PROCESSOR MATCHES "amd64") OR (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64") OR (CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64"))

        if(TOOB_AMD_OPTIMIZATIONS STREQUAL "SSE42")
            message(STATUS "Using SSE4.2 x86-64 optimizations")
            add_compile_options("-march=x86-64-v2")
            add_compile_definitions("TOOB_OPTIMIZATION_FLAGS=\"x86-64-v2\"")
        elseif(TOOB_AMD_OPTIMIZATIONS STREQUAL "AVX")
            message(STATUS "Using AVX x86-64 optimizations")
            add_compile_options("-march=x86-64-v3")
            add_compile_definitions("TOOB_OPTIMIZATION_FLAGS=\"x86-64-v3\"")
        elseif(TOOB_AMD_OPTIMIZATIONS STREQUAL "AVX-512")
            message(STATUS "Using AVX-512 x86-64 optimizations")
            add_compile_options("-march=x86-64-v4")
            add_compile_definitions("TOOB_OPTIMIZATION_FLAGS=\"x86-64-v4\"")
        elseif(TOOB_AMD_OPTIMIZATIONS STREQUAL "DEFAULT")
            message(STATUS "Using default x86-64 optimizations")
             # default compile options = -march="x86-64": SSE, and SSE2 only
            add_compile_definitions("TOOB_OPTIMIZATION_FLAGS=\"default\"")
             
        else()
            message(FATAL_ERROR "Invalid TOOB_AMD_OPTIMIZATIONS='${TOOB_AMD_OPTIMIZATIONS}'. Allowed values: DEFAULT, SSE42, AVX, AVX-512")
        endif()
    endif()
endif()

if (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    add_compile_options("-Wno-unknown-warning-option")
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
