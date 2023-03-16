# CMakeList.txt : CMake project for ToobAmp, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.18)

set (CMAKE_CXX_STANDARD 20)

set(PROFILER 1)  # enables google profiler in ConvolutionReverbTest


# message(STATUS "CMAKE_CXX_FLAGS_RELEASE: ${CMAKE_CXX_FLAGS_RELEASE}")
# message(STATUS "CMAKE_CXX_FLAGS_DEBUG: ${CMAKE_CXX_FLAGS_DEBUG}")
# message(STATUS "CMAKE_CXX_FLAGS_RELWITHDEBINFO: ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")


if (CMAKE_COMPILER_IS_GNUCXX)
    set (GCC_EXTRA_FLAGS "-Wno-psabi -Werror -Wall -pedantic -Wno-reorder --param=max-vartrack-size=200000")

    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -D_GLIBCXX_DEBUG")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -DNDEBUG -g")
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    #set(GCC_EXTRA_FLAGS "${GCC_EXTRA_FLAGS} -D_GLIBCXX_ASSERTS")
    #set(GCC_EXTRA_FLAGS "${GCC_EXTRA_FLAGS} -D_GLIBCXX_DEBUG")
    endif()

    SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${GCC_EXTRA_FLAGS}")
endif()


if (CMAKE_BUILD_TYPE STREQUAL "Debug")
   add_compile_definitions("DEBUG")
endif()
# Add resource files.
add_custom_target(copy-files ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/ToobAmp.lv2 ${CMAKE_CURRENT_BINARY_DIR}
    )



# Add source to this project's executable.
add_library(ToobAmp  SHARED 
        CircularBuffer.h
        ToobFreeverb.cpp ToobFreeverb.h
        ToobDelay.cpp ToobDelay.h
        ToobChorus.cpp ToobChorus.h
        ToobTuner.cpp ToobTuner.h
        InputStage.cpp InputStage.h Plugin.cpp Lv2Plugin.h MidiProcessor.h MidiProcessor.cpp std.h 
        InputPort.h CabSim.h CabSim.cpp CombFilter.h DbDezipper.cpp DbDezipper.h


        LsNumerics/InterpolatingDelay.cpp LsNumerics/InterpolatingDelay.hpp
        Ce2Chorus.cpp Ce2Chorus.hpp
        Filters/AudioFilter2.h
         Filters/FilterCoefficients2.h Filters/AudioFilter2.cpp Lv2Plugin.cpp OutputPort.h
         ToneStack.cpp ToneStack.h GainSection.cpp GainSection.h IDelay.h SagProcessor.h
         NoiseGate.cpp NoiseGate.h
         GainStage.cpp GainStage.h
         WaveShapes.cpp WaveShapes.h
         PowerStage2.h PowerStage2.cpp
         SpectrumAnalyzer.h SpectrumAnalyzer.cpp
         NeuralModel.h NeuralModel.cpp
         ToobML.h ToobML.cpp
         json.hpp json.cpp
         Filters/FilterCoefficients.cpp Filters/FilterCoefficients.h
         Filters/Polynomial.cpp Filters/Polynomial.h
         Filters/DownsamplingLowpassFilter.cpp Filters/DownsamplingLowPassFilter.h
         Filters/AudioFilter3.cpp Filters/AudioFilter3.h
         Filters/LowPassFilter.cpp Filters/LowPassFilter.h
         Filters/AudioFilter.cpp Filters/AudioFilter.h
         Filters/ChebyshevDownsamplingFilter.h
         Filters/ChebyshevDownsamplingFilter.cpp
         iir/Biquad.cpp
         iir/RBJ.cpp
         iir/State.h
         iir/Custom.cpp
         iir/Biquad.h
         iir/Cascade.h
         iir/Types.h
         iir/PoleFilter.cpp
         iir/Common.h
         iir/PoleFilter.h
         iir/Layout.h
         iir/RBJ.h
         iir/MathSupplement.h
         iir/Cascade.cpp
         iir/Butterworth.cpp
         iir/ChebyshevII.h
         iir/ChebyshevI.cpp
         iir/ChebyshevI.h
         iir/Butterworth.h
         iir/Custom.h
         iir/ChebyshevII.cpp
         
         LsNumerics/Freeverb.cpp LsNumerics/Freeverb.hpp
         LsNumerics/ToneStackFilter.cpp LsNumerics/ToneStackFilter.h
         LsNumerics/LsMath.hpp LsNumerics/LsMath.cpp
         LsNumerics/PiecewiseChebyshevApproximation.hpp
         LsNumerics/LsChebyshevApproximation.hpp
         LsNumerics/LsPolynomial.hpp
         LsNumerics/InPlaceBilinearFilter.h
         LsNumerics/BaxandallToneStack.hpp
         LsNumerics/LsChebyshevPolynomial.cpp
         LsNumerics/Fft.hpp
         LsNumerics/Fft.cpp
         LsNumerics/Window.hpp
         LsNumerics/LsChebyshevPolynomial.hpp
         LsNumerics/LsRationalPolynomial.cpp
         LsNumerics/TubeStageApproximation.cpp
         LsNumerics/PiecewiseChebyshevApproximation.cpp
         LsNumerics/BaxandallToneStack.cpp
         LsNumerics/TubeStageApproximation.hpp
         LsNumerics/LsRationalPolynomial.hpp
         LsNumerics/LsPolynomial.cpp
         LsNumerics/PitchDetector.hpp
         LsNumerics/PitchDetector.cpp
         
         )

target_link_libraries(ToobAmp RTNeural dl)
target_include_directories(ToobAmp PRIVATE
    ../modules/RTNeural
)

# Tests.
add_executable(FftTest 
    LsNumerics/FftTest.cpp 
    LsNumerics/CacheInfo.hpp
    LsNumerics/Fft.hpp 
    LsNumerics/Fft.cpp
    LsNumerics/StagedFft.hpp 
    LsNumerics/StagedFft.cpp
    LsNumerics/Window.hpp 
    TestAssert.hpp
)

add_test(FftTest FftTest)

add_executable(BaxandallToneStackTest 
    TestAssert.hpp
    LsNumerics/BaxandallToneStackTest.cpp 
    LsNumerics/BaxandallToneStack.cpp LsNumerics/BaxandallToneStack.hpp
    LsNumerics/LsPolynomial.cpp LsNumerics/LsPolynomial.hpp
    )

add_test(BaxandallToneStackTest BaxandallToneStackTest)



add_library(BalancedConvolution STATIC
    LsNumerics/BinaryReader.hpp
    LsNumerics/BinaryReader.cpp
    LsNumerics/BinaryWriter.cpp
    LsNumerics/BinaryWriter.hpp

    LsNumerics/FftConvolution.cpp
    LsNumerics/FftConvolution.hpp
    LsNumerics/StagedFft.cpp
    LsNumerics/StagedFft.hpp
    LsNumerics/ConvolutionReverb.cpp
    LsNumerics/ConvolutionReverb.hpp
    LsNumerics/BalancedConvolution.cpp
    LsNumerics/BalancedConvolution.hpp

    LsNumerics/SynchronizedDelayLine.hpp
    LsNumerics/SynchronizedDelayLine.cpp
)

target_link_libraries(BalancedConvolution pthread boost_iostreams.a z.a)


add_executable(GenerateFftPlans
    LsNumerics/GenerateFftPlans.cpp
    CommandLineParser.hpp
)
target_link_libraries(GenerateFftPlans
    PRIVATE BalancedConvolution

)



add_executable(ConvolutionReverbTest
    TestAssert.hpp
    CommandLineParser.hpp
    LsNumerics/ConvolutionReverbTest.cpp
    
)


if(PROFILER)
    target_link_libraries(ConvolutionReverbTest PRIVATE  BalancedConvolution profiler)
    target_compile_definitions(ConvolutionReverbTest PRIVATE "WITHGPERFTOOLS")    
else()
    target_link_libraries(ConvolutionReverbTest PRIVATE BalancedConvolution)
endif()

add_test(ConvolutionReverbTest ConvolutionReverbTest "--build")

add_executable(Ce2ChorusTest 
    TestAssert.hpp

    Ce2ChorusTest.cpp
    Ce2Chorus.cpp Ce2Chorus.hpp
    Filters/LowPassFilter.cpp Filters/LowPassFilter.h
    Filters/AudioFilter2.cpp Filters/AudioFilter2.h
    Filters/AudioFilter.cpp Filters/AudioFilter.h
    
    LsNumerics/InterpolatingDelay.cpp LsNumerics/InterpolatingDelay.hpp 
    LsNumerics/LsMath.hpp LsNumerics/LsMath.cpp
    Filters/ChebyshevDownsamplingFilter.cpp Filters/ChebyshevDownsamplingFilter
    iir/Biquad.cpp
    iir/Biquad.h
    iir/Cascade.h
    iir/Types.h
    iir/PoleFilter.cpp
    iir/Common.h
    iir/PoleFilter.h
    iir/Layout.h
    iir/MathSupplement.h
    iir/Cascade.cpp
    iir/ChebyshevII.h
    iir/ChebyshevI.cpp
    iir/ChebyshevI.h
    iir/ChebyshevII.cpp


)

add_test(Ce2ChorusTest Ce2ChorusTest)

# check for missing linkages.
add_executable(linkageTest 
    linkageTest.cpp )

target_link_libraries(linkageTest ToobAmp)



add_executable(PitchDetectorTest 
    TestAssert.hpp

    LsNumerics/Fft.hpp 
    LsNumerics/Fft.cpp
    LsNumerics/Window.hpp 
    LsNumerics/PitchDetector.cpp LsNumerics/PitchDetector.hpp
    LsNumerics/IfPitchDetector.cpp LsNumerics/IfPitchDetector.hpp 
    LsNumerics/PitchDetectorTest.cpp 
    LsNumerics/LsMath.cpp LsNumerics/LsMath.hpp
)
add_test(PitchDetectorTest PitchDetectorTest)


add_test(LinkageTest linkageTest)


set_target_properties(ToobAmp PROPERTIES OUTPUT_NAME "ToobAmp")

add_dependencies(ToobAmp copy-files)


set_target_properties(ToobAmp PROPERTIES VERSION ${PROJECT_VERSION})

set_target_properties(ToobAmp PROPERTIES SOVERSION 0)

set_target_properties(ToobAmp PROPERTIES OUTPUT_NAME "ToobAmp")
set_target_properties(ToobAmp PROPERTIES PREFIX "")
set_property(TARGET ToobAmp PROPERTY CXX_STANDARD 17)

include(GNUInstallDirs)



install(TARGETS ToobAmp
    LIBRARY DESTINATION /usr/lib/lv2/ToobAmp.lv2
    )
# Copy all assets to resources file
INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ToobAmp.lv2/ DESTINATION /usr/lib/lv2/ToobAmp.lv2
        )

# TODO: Add tests if needed.
