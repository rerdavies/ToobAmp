# CMakeList.txt : CMake project for ToobAmp, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.18)

set (CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (DEFINED BUILD_MACHINE)
    message(STATUS "Google Profiler support disabled.}")
    set (PROFILER 0) #no google-perf install on build machine.
else()
    #set(PROFILER 1)  # enables google profiler in ConvolutionReverbTest, ProfileNeuralAmpModeler. Requires google-perftools-dev
    set(PROFILER 0)  # disables google profiler in ConvolutionReverbTest, ProfileNeuralAmpModeler
endif()

execute_process(COMMAND
  dpkg-architecture
    -qDEB_HOST_ARCH
  OUTPUT_VARIABLE
    CMAKE_DEB_HOST_ARCH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

# find_package(PkgConfig REQUIRED)
# pkg_check_modules(DBUS REQUIRED dbus-1)
# include_directories(${DBUS_INCLUDE_DIRS})
# link_directories(${DBUS_LIBRARY_DIRS})




if (CMAKE_COMPILER_IS_GNUCXX)

    if (CMAKE_DEB_HOST_ARCH STREQUAL "arm64")
        set(CXX_ARCH_FLAGS "-mtune=cortex-a72") # raspberry pi 4. cortex-a72
    else()
        # MMX, SSE, SSE2 are enabled by default.
        
        # Minimum architecture for AVX: May exclude pentium4 mini-pcs, and core2 machines.
        # set(CXX_ARCH_FLAGS "-march=sandybridge -mtune=sandybridge" )  
    endif()


    set (GCC_EXTRA_FLAGS "-Wno-psabi -Werror -Wall -pedantic -Wno-reorder --param=max-vartrack-size=400000")

    set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -DNDEBUG ${CXX_ARCH_FLAGS}")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -D_GLIBCXX_DEBUG ${CXX_ARCH_FLAGS}")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-Ofast -DNDEBUG -DRELWITHDEBINFO -g ${CXX_ARCH_FLAGS}")
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    endif()

    SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${GCC_EXTRA_FLAGS}")
endif()

message(STATUS "CMAKE_DEB_HOST_ARCH: ${CMAKE_DEB_HOST_ARCH}")
message(STATUS "CMAKE_CXX_FLAGS_DEBUG: ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "CMAKE_CXX_FLAGS_RELEASE: ${CMAKE_CXX_FLAGS_RELEASE}")


if (CMAKE_BUILD_TYPE STREQUAL "Debug")
   add_compile_definitions("DEBUG")
endif()
# Add resource files.
add_custom_target(copy-files ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/ToobAmp.lv2 ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2
    )

set(NAM_SOURCES	
    ../modules/NeuralAmpModelerCore/NAM/activations.h
    ../modules/NeuralAmpModelerCore/NAM/activations.cpp
    ../modules/NeuralAmpModelerCore/NAM/version.h
    ../modules/NeuralAmpModelerCore/NAM/lstm.h
    ../modules/NeuralAmpModelerCore/NAM/lstm.cpp
    ../modules/NeuralAmpModelerCore/NAM/dsp.h
    ../modules/NeuralAmpModelerCore/NAM/dsp.cpp
    ../modules/NeuralAmpModelerCore/NAM/get_dsp.cpp
    ../modules/NeuralAmpModelerCore/NAM/util.cpp
    ../modules/NeuralAmpModelerCore/NAM/util.h
    ../modules/NeuralAmpModelerCore/NAM/wavenet.cpp
    ../modules/NeuralAmpModelerCore/NAM/wavenet.h
    ../modules/NeuralAmpModelerCore/NAM/convnet.cpp
    ../modules/NeuralAmpModelerCore/NAM/convnet.h

    ../modules/NeuralAmpModelerCore/dsp/dsp.cpp
    ../modules/NeuralAmpModelerCore/dsp/dsp.h
    ../modules/NeuralAmpModelerCore/dsp/ImpulseResponse.cpp
    ../modules/NeuralAmpModelerCore/dsp/ImpulseResponse.h
    namFixes/NamDSP.cpp 
    namFixes/NamDSP.h
    namFixes/NoiseGate.cpp
    namFixes//NoiseGate.h
    ../modules/NeuralAmpModelerCore/dsp/RecursiveLinearFilter.cpp
    ../modules/NeuralAmpModelerCore/dsp/RecursiveLinearFilter.h
    ../modules/NeuralAmpModelerCore/dsp/Resample.h
    ../modules/NeuralAmpModelerCore/dsp/wav.cpp
    ../modules/NeuralAmpModelerCore/dsp/wav.h

)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Intel")
    set(NAM_EXTRA_FLAGS -fPIC -Wno-sign-compare -Wno-pedantic -Wno-deprecated-declarations -Wno-unused-but-set-variable -Wno-effc++)
endif()

add_library(namSources STATIC ${NAM_SOURCES})
target_compile_options(namSources PRIVATE ${NAM_EXTRA_FLAGS} )
target_compile_definitions(namSources PUBLIC NAM_SAMPLE_FLOAT)
target_include_directories(namSources PUBLIC
../modules/NeuralAmpModelerCore
../modules/NeuralAmpModelerCore/Dependencies/eigen
../modules/NeuralAmpModelerCore/Dependencies/nlohmann
)

# disable vartracking for TubeStageApproximation to avoid insane compile times.
set_source_files_properties(LsNumerics/TubeStageApproximation.cpp PROPERTIES COMPILE_FLAGS "-fno-var-tracking ")
set_source_files_properties(NeuralAmpModeler.cpp PROPERTIES COMPILE_FLAGS "-Wno-pedantic")

# Add source to this project's executable.
add_library(ToobAmp  SHARED
        NeuralAmpModeler.cpp NeuralAmpModeler.h
        nam_architecture.hpp

        lv2ext/filedialog.h

        FlacReader.cpp FlacReader.hpp
        LsNumerics/SectionExecutionTrace.hpp LsNumerics/SectionExecutionTrace.cpp
        util.hpp util.cpp
        SvgPathWriter.hpp
        SvgPathWriter.cpp

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

        LsNumerics/AudioThreadToBackgroundQueue.hpp
        LsNumerics/AudioThreadToBackgroundQueue.cpp
        LsNumerics/LocklessQueue.hpp
        LsNumerics/LocklessQueue.cpp
    
        LsNumerics/FixedDelay.hpp



        ToobConvolutionReverb.cpp
        ToobConvolutionReverb.h
        CircularBuffer.h
        ToobFreeverb.cpp ToobFreeverb.h
        ToobDelay.cpp ToobDelay.h
        ToobChorus.cpp ToobChorus.h
        ToobFlanger.cpp ToobFlanger.h
        ToobTuner.cpp ToobTuner.h
        InputStage.cpp InputStage.h Plugin.cpp Lv2Plugin.h MidiProcessor.h MidiProcessor.cpp std.h 
        InputPort.h CabSim.h CabSim.cpp CombFilter.h DbDezipper.cpp DbDezipper.h
        WavReader.hpp
        WavReader.cpp
        WavWriter.hpp
        WavWriter.cpp
        WavGuid.hpp
        WavGuid.cpp
        WavConstants.hpp WavConstants.cpp
        AudioData.cpp
        AudioData.hpp

        LsNumerics/InterpolatingDelay.cpp LsNumerics/InterpolatingDelay.hpp
        Ce2Chorus.cpp Ce2Chorus.hpp
        Tf2Flanger.cpp Tf2Flanger.hpp
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
         Filters/ShelvingLowCutFilter2.h Filters/ShelvingLowCutFilter2.cpp
         Filters/FilterCoefficients.cpp Filters/FilterCoefficients.h
         Filters/Polynomial.cpp Filters/Polynomial.h
         Filters/DownsamplingLowpassFilter.cpp Filters/DownsamplingLowPassFilter.h
         Filters/AudioFilter3.cpp Filters/AudioFilter3.h
         Filters/LowPassFilter.cpp Filters/LowPassFilter.h
         Filters/HighPassFilter.cpp Filters/HighPassFilter.h
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
         LsNumerics/PiecewiseChebyshevApproximation.cpp
         LsNumerics/LsChebyshevApproximation.hpp
         LsNumerics/LsChebyshevApproximation.cpp
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

target_link_libraries(ToobAmp 
    #${DBUS_LIBRARIES}
    RTNeural
    namSources
    dl pthread
    boost_iostreams
    z
    ogg
    FLAC
    FLAC++
    )

    target_include_directories(ToobAmp PRIVATE
    ../modules/RTNeural 
    ../modules/NeuralAmpModelerCore/Dependencies/eigen
    ../modules/NeuralAmpModelerCore/Dependencies/nlohmann
    ../modules/NeuralAmpModelerCore
    ../modules    
)
set_property(TARGET ToobAmp PROPERTY CXX_STANDARD 20)

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

add_executable(ResamplerTest 
    LsNumerics/MotorolaResampler.hpp
    LsNumerics/ResamplerTest.cpp
    AudioData.cpp AudioData.hpp
    WavWriter.cpp WavWriter.hpp
    WavGuid.cpp WavGuid.hpp
    Filters/ChebyshevDownsamplingFilter.cpp Filters/ChebyshevDownsamplingFilter.cpp
    iir/ChebyshevI.h
    iir/ChebyshevI.cpp
    iir/Biquad.cpp
    iir/Biquad.h
    iir/Cascade.h
    iir/Types.h
    iir/PoleFilter.cpp
    iir/Common.h
    iir/PoleFilter.h
    iir/Cascade.cpp
    iir/Biquad.cpp
    WavConstants.cpp
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
    LsNumerics/SectionExecutionTrace.hpp LsNumerics/SectionExecutionTrace.cpp

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

    LsNumerics/AudioThreadToBackgroundQueue.hpp
    LsNumerics/AudioThreadToBackgroundQueue.cpp
    LsNumerics/LocklessQueue.hpp
    LsNumerics/LocklessQueue.cpp

    LsNumerics/FixedDelay.hpp

    util.hpp util.cpp
    #rtkit.h rtkit.cpp

    
)

target_link_libraries(BalancedConvolution 
    #${DBUS_LIBRARIES} 
    pthread boost_iostreams.a z.a)



add_executable(ConvolutionReverbTest
    LsNumerics/SectionExecutionTrace.hpp LsNumerics/SectionExecutionTrace.cpp

    util.hpp util.cpp
    TestAssert.hpp
    CommandLineParser.hpp
    LsNumerics/LagrangeInterpolator.hpp
    AudioData.hpp
    AudioData.cpp
    WavConstants.hpp WavConstants.cpp
    WavGuid.hpp WavGuid.cpp
    iir/ChebyshevI.cpp
    iir/ChebyshevI.h
    iir/ChebyshevI.cpp
    iir/Cascade.cpp
    iir/Biquad.cpp
    Filters/ChebyshevDownsamplingFilter.cpp
    LsNumerics/ConvolutionReverbTest.cpp
    iir/PoleFilter.cpp
    WavReader.hpp WavReader.cpp
    WavWriter.hpp WavWriter.cpp
    
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
    Tf2Flanger.cpp Tf2Flanger.hpp
    Filters/LowPassFilter.cpp Filters/LowPassFilter.h
    Filters/ShelvingLowCutFilter2.cpp Filters/ShelvingLowCutFilter2.h
    Filters/HighPassFilter.cpp Filters/HighPassFilter.h
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

target_link_libraries(linkageTest ToobAmp z.a ogg.a FLAC.a FLAC++.a)



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

add_executable(ProfileNeuralAmpModeler 
    ProfileNeuralAmpModeler.cpp
) 


target_include_directories(ProfileNeuralAmpModeler PRIVATE
../modules/NeuralAmpModelerCore/Dependencies/eigen
../modules/NeuralAmpModelerCore/Dependencies/nlohmann
../modules    
)

if(PROFILER)
    target_link_libraries(ProfileNeuralAmpModeler PRIVATE  ToobAmp profiler)
    target_compile_definitions(ProfileNeuralAmpModeler PRIVATE "WITHGPERFTOOLS")    
else()
    target_link_libraries(ProfileNeuralAmpModeler PRIVATE ToobAmp)
endif()




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
INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ToobAmp.lv2/ DESTINATION /usr/lib/lv2/ToobAmp.lv2)
INSTALL(DIRECTORY ${CMAKE_SOURCE_DIR}/fftplans.gz/ DESTINATION /usr/lib/lv2/ToobAmp.lv2/fftplans.gz)
INSTALL(DIRECTORY ${CMAKE_SOURCE_DIR}/impulseFiles/reverb/ DESTINATION /usr/lib/lv2/ToobAmp.lv2/impulseFiles/reverb)
INSTALL(DIRECTORY ${CMAKE_SOURCE_DIR}/impulseFiles/CabIR/ DESTINATION /usr/lib/lv2/ToobAmp.lv2/impulseFiles/CabIR)
        

# TODO: Add tests if needed.
