# CMakeList.txt : CMake project for ToobAmp, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.9)

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)


# Add source to this project's executable.
add_executable(test "Test.cpp" "Test.h" "LoadTest.h" "LoadTest.cpp" "Lv2Api.h" "Lv2Api.cpp" "MapFeature.h" "MapFeature.cpp" "InputControl.h" 
        "HostedLv2Plugin.h" "HostedLv2Plugin.cpp" "OutputControl.h" "Lv2Exception.h" "Lv2Host.h" "Lv2Host.cpp" "ScheduleFeature.h" "ScheduleFeature.cpp" "LogFeature.h" "LogFeature.cpp")
add_dependencies(test ToobAmp)
target_link_libraries(test ToobAmp)
if (WIN32) 
    target_link_libraries(test kernel32.lib)
else()
    target_link_libraries(test dl)

endif()

set_target_properties(test PROPERTIES VERSION ${PROJECT_VERSION})

include(GNUInstallDirs)


# TODO: Add tests and install targets if needed.

