# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.9)


set(CMAKE_VERBOSE_MAKEFILE OFF CACHE BOOL "OFF")

project(ToobAmp VERSION 1.0.17 DESCRIPTION "TooBAmp LV2 Plugin")

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

set(CMAKE_CXX_STANDARD 20 CACHE STRING "Default C++ standard")
# Include sub-projects.
add_subdirectory ("src")
if (WIN32)
#	add_subdirectory ("Test")
endif()


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

# set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/debian/postinst;${CMAKE_CURRENT_SOURCE_DIR}/debian/prerm")


include(CPack)
