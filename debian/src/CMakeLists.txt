
set (DEBIAN_COPYRIGHT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../../build/debian/copyright)
set (LICENSE_MD_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../../build/src/ToobAmp.lv2/LICENSE.md)

add_executable(processCopyrights  
    processCopyrights.cpp
    CommandLineParser.hpp
    ss.hpp
)
add_custom_command(OUTPUT ${DEBIAN_COPYRIGHT_FILE} ${LICENSE_MD_FILE} 
    COMMAND "./makeCopyrights"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
    DEPENDS "$<TARGET_FILE:processCopyrights>" ../copyright-fixups ../copyrightReplacements.sed ../copyrights-base
    COMMENT "Updating copyright notices."
)


add_custom_target (
    CopyrightBuild ALL
    DEPENDS ${DEBIAN_COPYRIGHT_FILE} 
    DEPENDS ${LICENSE_MD_FILE}
)

install(   
     FILES ${DEBIAN_COPYRIGHT_FILE} DESTINATION /usr/share/doc/toobamp
)




