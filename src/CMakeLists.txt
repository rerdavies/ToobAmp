# CMakeList.txt : CMake project for ToobAmp, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.18)

set (GCC_EXTRA_FLAGS "-Wno-psabi")

SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${GCC_EXTRA_FLAGS}")
# Add resource files.
add_custom_target(copy-files ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/ToobAmp.lv2 ${CMAKE_CURRENT_BINARY_DIR}
    )




# Add source to this project's executable.
add_library(ToobAmp  SHARED "InputStage.cpp" "InputStage.h" "Plugin.cpp" "Lv2Plugin.h" "MidiProcessor.h" "MidiProcessor.cpp" "std.h" "lv2_rgext.h" 
        "InputPort.h" "ToobMath.h" "ToobMath.cpp" "CabSim.h" "CabSim.cpp" "CombFilter.h" "DbDezipper.cpp" "DbDezipper.h"
        "Filters/AudioFilter2.h"
         "Filters/FilterCoefficients2.h" "Filters/AudioFilter2.cpp" "Lv2Plugin.cpp" "OutputPort.h"
         "ToneStack.cpp" "ToneStack.h" "GainSection.cpp" "GainSection.h" "PowerStage.cpp" "PowerStage.h" "IDelay.h" "SagProcessor.h"
         NoiseGate.cpp NoiseGate.h
         GainStage.cpp GainStage.h
         Filters/FilterCoefficients.cpp Filters/FilterCoefficients.h
         Filters/ToneStackFilter.cpp Filters/ToneStackFilter.h
         Filters/Polynomial.cpp Filters/Polynomial.h
         Filters/DownsamplingLowpassFilter.cpp Filters/DownsamplingLowPassFilter.h
         Filters/AudioFilter3.cpp Filters/AudioFilter3.h
         Filters/LowPassFilter.cpp Filters/LowPassFilter.h
        "Filters/AudioFilter.cpp" Filters/AudioFilter.h

         )

set_target_properties(ToobAmp PROPERTIES OUTPUT_NAME "ToobAmp")

add_dependencies(ToobAmp copy-files)

set_target_properties(ToobAmp PROPERTIES VERSION ${PROJECT_VERSION})

set_target_properties(ToobAmp PROPERTIES SOVERSION 0)

set_target_properties(ToobAmp PROPERTIES OUTPUT_NAME "ToobAmp")
set_target_properties(ToobAmp PROPERTIES PREFIX "")

include(GNUInstallDirs)


install(TARGETS ToobAmp
    LIBRARY DESTINATION /usr/lib/lv2/ToobAmp.lv2
    )
# Copy all assets to resources file
INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ToobAmp.lv2/ DESTINATION /usr/lib/lv2/ToobAmp.lv2
        )

# TODO: Add tests if needed.
