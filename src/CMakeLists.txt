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


set (FLAC_LIBS  FLAC++.a FLAC.a ogg.a)



message(STATUS "src: CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID}")

if (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")

    if (CMAKE_DEB_HOST_ARCH STREQUAL "arm64")
        set(CXX_ARCH_FLAGS "-mtune=cortex-a72") # raspberry pi 4. cortex-a72
    else()
        # MMX, SSE, SSE2 are enabled by default.
        
        # Minimum architecture for AVX: May exclude pentium4 mini-pcs, and core2 machines.
        # set(CXX_ARCH_FLAGS "-march=sandybridge -mtune=sandybridge" )  
    endif()


    set (GCC_EXTRA_FLAGS "-Wno-psabi -Werror -Wall -pedantic -Wno-reorder -Wno-restrict ")   # --param=max-vartrack-size=400000

    set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -DNDEBUG ${CXX_ARCH_FLAGS}")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -D_GLIBCXX_DEBUG ${CXX_ARCH_FLAGS}")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-Ofast -DNDEBUG -DRELWITHDEBINFO -g ${CXX_ARCH_FLAGS}")
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    endif()

    SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${GCC_EXTRA_FLAGS}")
endif()

message(STATUS "src: CMAKE_DEB_HOST_ARCH: ${CMAKE_DEB_HOST_ARCH}")
message(STATUS "src: CMAKE_CXX_FLAGS_DEBUG: ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "src: CMAKE_CXX_FLAGS_RELEASE: ${CMAKE_CXX_FLAGS_RELEASE}")


if (CMAKE_BUILD_TYPE STREQUAL "Debug")
   add_compile_definitions("DEBUG")
endif()


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
    set(NAM_EXTRA_FLAGS -fPIC -Wno-sign-compare -Wno-pedantic -Wno-deprecated-declarations -Wno-unused-but-set-variable
      -Wno-deprecated-enum-enum-conversion 
      -Wno-effc++)
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
        InputPort.h CabSim.h CabSim.cpp CombFilter2.h DbDezipper.cpp DbDezipper.h
        CombFilter2.cpp
        restrict.hpp
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

target_link_options(ToobAmp PRIVATE
         -Wl,-z,nodelete 
    -Wl,-uFLAC__stream_decoder_get_blocksize   # demand-link plugin.
    -Wl,-uogg_sync_init   # demand-link plugin.
)


target_link_libraries(ToobAmp 
    #${DBUS_LIBRARIES}
    RTNeural
    namSources
    dl pthread
    boost_iostreams
    ${FLAC_LIBS}
    )

    target_include_directories(ToobAmp PRIVATE
    ../modules/RTNeural 
    ../modules/NeuralAmpModelerCore/Dependencies/eigen
    ../modules/NeuralAmpModelerCore/Dependencies/nlohmann
    ../modules/NeuralAmpModelerCore
    ../modules    
)
set_property(TARGET ToobAmp PROPERTY CXX_STANDARD 20)


####################################################


set(LV2C_INCLUDE_DIRS
   ${PROJECT_SOURCE_DIR}/modules/lv2cairo/src/lv2c/include
   ${PROJECT_SOURCE_DIR}/modules/lv2cairo/src/lv2c_ui/include
)

configure_file(ToobAmp.lv2/ttl.in/ToobNeuralAmpModeler.ttl.in ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/ToobNeuralAmpModeler.ttl)
add_custom_command(
    OUTPUT  ToobNeuralAmpModelerInfo.hpp          # Treated as relative to CMAKE_CURRENT_BINARY_DIR
    COMMAND generate_lv2c_plugin_info 
               http://two-play.com/plugins/toob-nam
              --ttl ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/manifest.ttl
              --out ${CMAKE_CURRENT_BINARY_DIR}/ToobNeuralAmpModelerInfo.hpp 
              --class NeuralAmpModelerInfo
    DEPENDS 
        ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/manifest.ttl 
        ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/ToobNeuralAmpModeler.ttl    
        generate_lv2c_plugin_info
)




configure_file(ToobAmp.lv2/ttl.in/CabIR.ttl.in ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/CabIR.ttl)
add_custom_command(
    OUTPUT  CabIRInfo.hpp          # Treated as relative to CMAKE_CURRENT_BINARY_DIR
    COMMAND generate_lv2c_plugin_info 
               http://two-play.com/plugins/toob-cab-ir
              --ttl ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/manifest.ttl
              --out ${CMAKE_CURRENT_BINARY_DIR}/CabIRInfo.hpp 
              --class CabIRInfo
    DEPENDS 
        ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/manifest.ttl 
        ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/CabIR.ttl    
        generate_lv2c_plugin_info
)



configure_file(ToobAmp.lv2/ttl.in/ConvolutionReverb.ttl.in ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/ConvolutionReverb.ttl)
add_custom_command(
    OUTPUT  ConvolutionReverbInfo.hpp          # Treated as relative to CMAKE_CURRENT_BINARY_DIR
    COMMAND generate_lv2c_plugin_info 
               http://two-play.com/plugins/toob-convolution-reverb
              --ttl ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/manifest.ttl
              --out ${CMAKE_CURRENT_BINARY_DIR}/ConvolutionReverbInfo.hpp 
              --class ConvolutionReverbInfo
    DEPENDS 
        ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/manifest.ttl 
        ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/ConvolutionReverb.ttl    
        generate_lv2c_plugin_info
)

configure_file(ToobAmp.lv2/ttl.in/ConvolutionReverbStereo.ttl.in ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/ConvolutionReverbStereo.ttl)
add_custom_command(
    OUTPUT  ConvolutionReverbStereoInfo.hpp          # Treated as relative to CMAKE_CURRENT_BINARY_DIR
    COMMAND generate_lv2c_plugin_info 
               http://two-play.com/plugins/toob-convolution-reverb-stereo
              --ttl ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/manifest.ttl
              --out ${CMAKE_CURRENT_BINARY_DIR}/ConvolutionReverbStereoInfo.hpp 
              --class ConvolutionReverbStereoInfo
    DEPENDS 
        ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/manifest.ttl 
        ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/ConvolutionReverbStereo.ttl    
        generate_lv2c_plugin_info
)



configure_file(ToobAmp.lv2/ttl.in/ToobTuner.ttl.in ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/ToobTuner.ttl)
add_custom_command(
    OUTPUT  ToobTunerInfo.hpp          # Treated as relative to CMAKE_CURRENT_BINARY_DIR
    COMMAND generate_lv2c_plugin_info 
               http://two-play.com/plugins/toob-tuner
              --ttl ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/manifest.ttl
              --out ${CMAKE_CURRENT_BINARY_DIR}/ToobTunerInfo.hpp 
              --class ToobTunerInfo
    DEPENDS 
        ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/manifest.ttl 
        ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/ToobTuner.ttl    
        generate_lv2c_plugin_info
)

configure_file(ToobAmp.lv2/ttl.in/SpectrumAnalyzer.ttl.in ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/SpectrumAnalyzer.ttl)
add_custom_command(
    OUTPUT  SpectrumAnalyzerInfo.hpp          # Treated as relative to CMAKE_CURRENT_BINARY_DIR
    COMMAND generate_lv2c_plugin_info 
               http://two-play.com/plugins/toob-spectrum
              --ttl ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/manifest.ttl
              --out ${CMAKE_CURRENT_BINARY_DIR}/SpectrumAnalyzerInfo.hpp 
              --class SpectrumAnalyzerInfo
    DEPENDS 
        ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/manifest.ttl 
        ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/SpectrumAnalyzer.ttl    
        generate_lv2c_plugin_info
)

configure_file(ToobAmp.lv2/ttl.in/ToobML.ttl.in ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/ToobML.ttl)
add_custom_command(
    OUTPUT  ToobMLInfo.hpp          # Treated as relative to CMAKE_CURRENT_BINARY_DIR
    COMMAND generate_lv2c_plugin_info 
               http://two-play.com/plugins/toob-ml
              --ttl ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/manifest.ttl
              --out ${CMAKE_CURRENT_BINARY_DIR}/ToobMLInfo.hpp 
              --class ToobMLInfo
    DEPENDS 
        ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/manifest.ttl 
        ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/ToobML.ttl    
        generate_lv2c_plugin_info
)


configure_file(ToobAmp.lv2/ttl.in/ToobFreeverb.ttl.in ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/ToobFreeverb.ttl)
add_custom_command(
    OUTPUT  ToobFreeverbInfo.hpp          # Treated as relative to CMAKE_CURRENT_BINARY_DIR
    COMMAND generate_lv2c_plugin_info 
               http://two-play.com/plugins/toob-freeverb
              --ttl ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/manifest.ttl
              --out ${CMAKE_CURRENT_BINARY_DIR}/ToobFreeverbInfo.hpp 
              --class ToobFreeverbInfo
    DEPENDS 
        ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/manifest.ttl 
        ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/ToobFreeverb.ttl    
        generate_lv2c_plugin_info
)


configure_file(ToobAmpVersion.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/ToobAmpVersion.hpp)

configure_file(ToobAmp.lv2/ttl.in/ToneStack.ttl.in ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/ToneStack.ttl)
add_custom_command(
    OUTPUT  ToneStackInfo.hpp          # Treated as relative to CMAKE_CURRENT_BINARY_DIR
    COMMAND generate_lv2c_plugin_info 
               http://two-play.com/plugins/toob-tone-stack
              --ttl ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/manifest.ttl
              --out ${CMAKE_CURRENT_BINARY_DIR}/ToneStackInfo.hpp 
              --class ToneStackPluginInfo
    DEPENDS 
        ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/manifest.ttl 
        ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/ToneStack.ttl    
        generate_lv2c_plugin_info
)
configure_file(ToobAmp.lv2/ttl.in/ToobFlanger.ttl.in ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/ToobFlanger.ttl)
add_custom_command(
    OUTPUT  Tf2FlangerInfo.hpp          # Treated as relative to CMAKE_CURRENT_BINARY_DIR
    COMMAND generate_lv2c_plugin_info 
               http://two-play.com/plugins/toob-flanger
              --ttl ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/manifest.ttl
              --out ${CMAKE_CURRENT_BINARY_DIR}/Tf2FlangerInfo.hpp 
              --class Tf2FlangerPluginInfo
    DEPENDS 
        ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/manifest.ttl 
        ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/ToobFlanger.ttl    
        generate_lv2c_plugin_info
)

configure_file(ToobAmp.lv2/ttl.in/ToobFlangerStereo.ttl.in ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/ToobFlangerStereo.ttl)
add_custom_command(
    OUTPUT  Tf2FlangerStereoInfo.hpp          # Treated as relative to CMAKE_CURRENT_BINARY_DIR
    COMMAND generate_lv2c_plugin_info 
               http://two-play.com/plugins/toob-flanger-stereo
              --ttl ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/manifest.ttl
              --out ${CMAKE_CURRENT_BINARY_DIR}/Tf2FlangerStereoInfo.hpp 
              --class Tf2FlangerStereoPluginInfo
    DEPENDS 
        ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/manifest.ttl 
        ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/ToobFlangerStereo.ttl    
        generate_lv2c_plugin_info
)



configure_file(ToobAmp.lv2/ttl.in/manifest.ttl.in ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/manifest.ttl)

configure_file(ToobAmp.lv2/ttl.in/CabSim.ttl.in ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/CabSim.ttl)
add_custom_command(
    OUTPUT  CabSimInfo.hpp          # Treated as relative to CMAKE_CURRENT_BINARY_DIR
    COMMAND generate_lv2c_plugin_info 
               http://two-play.com/plugins/toob-cab-sim
              --ttl ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/manifest.ttl
              --out ${CMAKE_CURRENT_BINARY_DIR}/CabSimInfo.hpp 
              --class CabSimPluginInfo
    DEPENDS 
        ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/manifest.ttl 
        ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/CabSim.ttl    
        generate_lv2c_plugin_info
)

configure_file(ToobAmp.lv2/ttl.in/ToobDelay.ttl.in ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/ToobDelay.ttl)
add_custom_command(
    OUTPUT  ToobDelayInfo.hpp          # Treated as relative to CMAKE_CURRENT_BINARY_DIR
    COMMAND generate_lv2c_plugin_info 
               http://two-play.com/plugins/toob-delay
              --ttl ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/manifest.ttl
              --out ${CMAKE_CURRENT_BINARY_DIR}/ToobDelayInfo.hpp 
              --class ToobDelayPluginInfo
    DEPENDS 
        ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/manifest.ttl 
        ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/ToobDelay.ttl    
        generate_lv2c_plugin_info
)

configure_file(ToobAmp.lv2/ttl.in/ToobChorus.ttl.in ToobAmp.lv2/ToobChorus.ttl)
add_custom_command(
    OUTPUT  ToobChorusInfo.hpp          # Treated as relative to CMAKE_CURRENT_BINARY_DIR
    COMMAND generate_lv2c_plugin_info 
               http://two-play.com/plugins/toob-chorus
              --ttl ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/manifest.ttl
              --out ${CMAKE_CURRENT_BINARY_DIR}/ToobChorusInfo.hpp 
              --class ToobChorusPluginInfo
    DEPENDS 
        ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/manifest.ttl 
        ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/ToobChorus.ttl    
        generate_lv2c_plugin_info
)

configure_file(ToobAmp.lv2/ttl.in/InputStage.ttl.in ToobAmp.lv2/InputStage.ttl)
add_custom_command(
    OUTPUT  InputStageInfo.hpp          # Treated as relative to CMAKE_CURRENT_BINARY_DIR
    COMMAND generate_lv2c_plugin_info 
               http://two-play.com/plugins/toob-input_stage
              --ttl ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/manifest.ttl
              --out ${CMAKE_CURRENT_BINARY_DIR}/InputStageInfo.hpp 
              --class InputStagePluginInfo
    DEPENDS 
        ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/manifest.ttl 
        ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/InputStage.ttl    
        generate_lv2c_plugin_info
)



add_library(ToobAmpUI  SHARED
    ${CMAKE_CURRENT_BINARY_DIR}/ToobNeuralAmpModelerInfo.hpp 
    ToobNeuralAmpModelerUi.cpp

    ${CMAKE_CURRENT_BINARY_DIR}/CabIRInfo.hpp 
    CabIRUi.cpp

    ${CMAKE_CURRENT_BINARY_DIR}/ConvolutionReverbStereoInfo.hpp 
    ConvolutionReverbStereoUi.cpp

    ${CMAKE_CURRENT_BINARY_DIR}/ConvolutionReverbInfo.hpp 
    ConvolutionReverbUi.cpp

    ${CMAKE_CURRENT_BINARY_DIR}/ToobTunerInfo.hpp 
    ToobTunerUi.cpp

    ${CMAKE_CURRENT_BINARY_DIR}/SpectrumAnalyzerInfo.hpp 
    SpectrumAnalyzerUi.cpp
    
    ${CMAKE_CURRENT_BINARY_DIR}/ToobMLInfo.hpp 
    ToobMlUi.cpp

    ${CMAKE_CURRENT_BINARY_DIR}/ToobFreeverbInfo.hpp 
    ToobFreeverbUi.cpp


    ${CMAKE_CURRENT_BINARY_DIR}/Tf2FlangerInfo.hpp 
    Tf2FlangerUi.cpp

    ${CMAKE_CURRENT_BINARY_DIR}/Tf2FlangerStereoInfo.hpp 
    Tf2FlangerStereoUi.cpp

    ${CMAKE_CURRENT_BINARY_DIR}/ToneStackInfo.hpp 
    ToneStackUi.cpp

    ${CMAKE_CURRENT_BINARY_DIR}/CabSimInfo.hpp 
    CabSimUi.cpp

    ${CMAKE_CURRENT_BINARY_DIR}/ToobChorusInfo.hpp 
    ToobChorusUi.cpp

    ${CMAKE_CURRENT_BINARY_DIR}/ToobDelayInfo.hpp 
    ToobDelayUi.cpp

    ${CMAKE_CURRENT_BINARY_DIR}/InputStageInfo.hpp 
    InputStageUi.cpp

    AboutDialog.hpp AboutDialog.cpp
    ToobUi.hpp ToobUi.cpp

)
set_target_properties(ToobAmpUI PROPERTIES OUTPUT_NAME "ToobAmpUI")
set_target_properties(ToobAmpUI PROPERTIES PREFIX "")

target_link_options(ToobAmpUI PRIVATE
    -Wl,-z,nodelete 
    # -Wl,-utoobChorusLinkage    # demand-link plugin.
    # -Wl,-ulinkInputStage    # demand-link plugin.
    # -Wl,-ulinkCabSimUi   # demand-link plugin.
    # -Wl,-ulinkToobDelayUi   # demand-link plugin.
    
    )



target_include_directories(
    ToobAmpUI PRIVATE
    ${LV2C_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(
    ToobAmpUI PRIVATE 
    lv2c lv2c_ui
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


add_executable(TestFlac 
    TestFlac.cpp
    FlacReader.hpp
    )

target_link_libraries(TestFlac PRIVATE  ToobAmp
)


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

add_executable(CombFilterTest
    CombFilterTest.cpp
    CombFilter2.cpp CombFilter2.h
    IDelay.h
    restrict.hpp
    LsNumerics/LsMath.cpp
)

add_test(CombFilterTest CombFilterTest)


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

add_executable(soLinkageTest 
    soLinkageTest.cpp 
)

target_link_libraries(soLinkageTest dl)



# check for missing linkages.
add_executable(linkageTest 

    linkageTest.cpp )

target_link_libraries(linkageTest ToobAmp ${FLAC_LIBS})



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
    target_link_libraries(ProfileNeuralAmpModeler PRIVATE  ToobAmp profiler ${FLAC_LIBS})
    target_compile_definitions(ProfileNeuralAmpModeler PRIVATE "WITHGPERFTOOLS")    
else()
    target_link_libraries(ProfileNeuralAmpModeler PRIVATE ToobAmp ${FLAC_LIBS})
endif()



set_target_properties(ToobAmp PROPERTIES VERSION ${PROJECT_VERSION})

set_target_properties(ToobAmp PROPERTIES SOVERSION 0)

set_target_properties(ToobAmp PROPERTIES OUTPUT_NAME "ToobAmp")
set_target_properties(ToobAmp PROPERTIES PREFIX "")

include(GNUInstallDirs)


install(TARGETS ToobAmp ToobAmpUI
    LIBRARY DESTINATION /usr/lib/lv2/ToobAmp.lv2
    )


add_executable(UiLinkageTest 

    UiLinkageTest.cpp 
    MapFeature.cpp MapFeature.h)

target_link_libraries(UiLinkageTest ToobAmpUI)

target_include_directories(
    UiLinkageTest  PRIVATE
    ${LV2C_INCLUDE_DIRS}
)


# Copy all assets to resources file
INSTALL(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/ToobAmp.lv2/ DESTINATION /usr/lib/lv2/ToobAmp.lv2)
INSTALL(DIRECTORY ${CMAKE_SOURCE_DIR}/src/ToobAmp.lv2/ttl/ DESTINATION /usr/lib/lv2/ToobAmp.lv2)
INSTALL(DIRECTORY ${CMAKE_SOURCE_DIR}/impulseFiles/reverb/ DESTINATION /usr/lib/lv2/ToobAmp.lv2/impulseFiles/reverb)
INSTALL(DIRECTORY ${CMAKE_SOURCE_DIR}/impulseFiles/CabIR/ DESTINATION /usr/lib/lv2/ToobAmp.lv2/impulseFiles/CabIR)
INSTALL(DIRECTORY ${CMAKE_SOURCE_DIR}/src/ToobAmp.lv2/models/ DESTINATION /usr/lib/lv2/ToobAmp.lv2/models)
INSTALL(DIRECTORY ${CMAKE_SOURCE_DIR}/src/resources/ DESTINATION /usr/lib/lv2/ToobAmp.lv2/resources)
INSTALL(DIRECTORY ${PROJECT_SOURCE_DIR}/modules/lv2cairo/resources/ DESTINATION /usr/lib/lv2/ToobAmp.lv2/resources)
INSTALL(FILES ${PROJECT_SOURCE_DIR}/MPL-2.0.md DESTINATION /usr/lib/lv2/ToobAmp.lv2)


# TODO: Add tests if needed.
