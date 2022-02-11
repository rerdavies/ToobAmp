# Typescript compile.

add_custom_command(
    OUTPUT stub.d.js
    COMMAND tsc --build
    WORKING_DIRECTORY
        ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS
        src/ToobAmp2.tsx
)
add_custom_target(ReactComponents 
    DEPENDS stub.d.js
)