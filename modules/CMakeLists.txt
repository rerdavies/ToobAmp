set(RTNEURAL_XSIMD OFF CACHE BOOL "Use RTNeural with this backend" FORCE)

set(BUILD_STATIC_RTNEURAL ON CACHE BOOL "Build RTNeural as a static library" FORCE)
set(BUILD_NAM_CORE ON BOOL)

add_subdirectory("NeuralAudio")
# add_subdirectory("RTNeural")
add_subdirectory("lv2cairo")
# add_subdirectory("NeuralAmpModelerCore")
