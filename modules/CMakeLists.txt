set(RTNEURAL_XSIMD OFF CACHE BOOL "Use RTNeural with this backend" FORCE)

set(BUILD_STATIC_RTNEURAL OFF CACHE BOOL "Build RTNeural as a static library" FORCE)
set(BUILD_NAMCORE OFF CACHE BOOL "Build RTNeural as a static library" FORCE)

add_subdirectory("NeuralAudio")
# add_subdirectory("RTNeural")
add_subdirectory("lv2cairo")
# add_subdirectory("NeuralAmpModelerCore")
