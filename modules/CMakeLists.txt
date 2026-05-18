

add_subdirectory("lv2cairo")
 
# Force options for NeuralAudio
set(BUILD_NAM_CORE OFF CACHE BOOL "Build NeuralAmpModelerCore" FORCE)
set(BUILD_STATIC_RTNEURAL ON CACHE BOOL "Build RTNeural as a static library" FORCE)
set(RTNEURAL_XSIMD OFF CACHE BOOL "Use RTNeural with this backend" FORCE)

add_subdirectory("NeuralAudio")

# add_subdirectory("RTNeural")
# add_subdirectory("NeuralAmpModelerCore")

add_subdirectory("libsamplerate")