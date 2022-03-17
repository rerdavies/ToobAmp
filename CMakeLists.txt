# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.9)


set(CMAKE_VERBOSE_MAKEFILE ON CACHE BOOL "ON")

project(ToobAmp VERSION 0.1.6 DESCRIPTION "TooBAmp LV2 Plugin")

include(CTest)
enable_testing()


if(WIN32)
	include_directories("C:/Users/rerda/source/lv2-1.18.2")
	add_compile_options("/std:c++17")
else()
	include_directories("/usr/local/include")
endif()

if (MSVC)
else()
    
    add_compile_options("-std=c++17")
endif()

# Include submodules
add_subdirectory("modules")

# debian copyright utils.
add_subdirectory("debian/src")

# Include sub-projects.
add_subdirectory ("src")
add_subdirectory ("react")
if (WIN32)
#	add_subdirectory ("Test")
endif()


set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Robin Davies <rerdavies@gmail.com>") # required
set(CPACK_PACKAGE_VENDOR "Robin Davies")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/debian/package_description.txt")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Lv2-based guitar effects pedal for Raspberry Pi")
set(CPACK_DEBIAN_PACKAGE_SECTION sound)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_PREDEPENDS "hostapd,  authbind" )
set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION TRUE)
#set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "arm64")
set(CPACK_PACKAGING_INSTALL_PREFIX /usr/local)
set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

# set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/debian/postinst;${CMAKE_CURRENT_SOURCE_DIR}/debian/prerm")


include(CPack)
