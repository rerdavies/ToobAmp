set(RTNEURAL_XSIMD ON CACHE BOOL "Use RTNeural with this backend" FORCE)
add_subdirectory("RTNeural")
add_subdirectory("lv2cairo")
# add_subdirectory("NeuralAmpModelerCore")
