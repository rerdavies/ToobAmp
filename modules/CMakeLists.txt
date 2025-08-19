set(RTNEURAL_XSIMD OFF CACHE BOOL "Use RTNeural with this backend" FORCE)

set(BUILD_STATIC_RTNEURAL OFF CACHE BOOL "Build RTNeural as a static library" FORCE)
set(BUILD_NAM_CORE OFF CACHE BOOL "Build NeuralAmpModelerCore" FORCE)
set(WAVENET_MATH "FastMath" CACHE STRING "WaveNet math functions" FORCE)
set(LSTM_MATH "FastMath" CACHE STRING "LSTM math functions" )

add_subdirectory("lv2cairo")
 
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelMinDependency")
    add_compile_options("-Ofast")
endif()

add_subdirectory("NeuralAudio")
# add_subdirectory("RTNeural")
# add_subdirectory("NeuralAmpModelerCore")

add_subdirectory("libsamplerate")