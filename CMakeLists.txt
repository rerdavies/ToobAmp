# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.9)
set(VERBOSE true)
set(CMAKE_VERBOSE_MAKEFILE ON)

project(ToobAmp VERSION 1.1.66 DESCRIPTION "TooB LV2 Guitar Effects Plugins")
# Semantic Version release type. e.g. "-alpha", "-beta3" or "" for a release.


set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(INSTALL_LV2CAIRO_TEST_PLUGIN OFF) # Install the lv2caoir test plugin.
option(WARNINGS_ARE_ERRORS ON) # Treat warnings as errors.
option(A76_OPTIMIZATION OFF) # Optimize for cortex-a76 (arm64 builds only)
option(TOOB_MULTI_ARCH_BUILD OFF) # install both a72 and a76 Toob plugins in the same package.


add_compile_definitions(NAM_SAMPLE_FLOAT=1)

project(ToobAmp VERSION 1.1.66 DESCRIPTION "TooB LV2 Guitar Effects Plugins")
# Semantic Version release type. e.g. "-alpha", "-beta3" or "" for a release.

set(PROJECT_RELEASE_QUALIFIER "")

set(TOOBAMP_SEMANTIC_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}${PROJECT_RELEASE_QUALIFIER}")

message(STATUS "ToobAmp Version: ${TOOBAMP_SEMANTIC_VERSION}")

execute_process(COMMAND
  dpkg-architecture
    -qDEB_HOST_ARCH
  OUTPUT_VARIABLE
    CMAKE_DEB_HOST_ARCH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)


if (CMAKE_DEB_HOST_ARCH STREQUAL "arm64")
    if (A76_OPTIMIZATION)
        message(STATUS "Using Cortex-A76 optimizations")
        add_compile_options("-mcpu=cortex-a76" "-mtune=cortex-a76")
    else()
        message(STATUS "Using Cortex-A72 optimizations")
        add_compile_options("-mcpu=cortex-a72" "-mtune=cortex-a72")
    endif()
endif()

include(CTest)
enable_testing()

set (CMAKE_CXX_STANDARD 20)

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
add_subdirectory ("src")

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
