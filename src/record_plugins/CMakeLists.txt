# CMakeList.txt : CMake project for ToobAmp, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.18)

set(PLUGIN_URI https://rerdavies.github.io/lv2c/download_test_plugin)
set(LV2_INSTALL_PATH lv2tk_download_test.lv2)  # relative to /usr/lib/lv2, or /usr/local/lib/lv2.
set(PLUGIN_UI_URI ${PLUGIN_URI}-ui )
set(LV2_SO_NAME lv2tk_download_test)
set(LV2_UI_SO_NAME ${LV2_SO_NAME}_ui)


configure_file(ttl/manifest.ttl.in manifest.ttl)
configure_file(ttl/plugin.ttl.in lv2tk_download_test.ttl)

set (PLUGIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib/lv2/${LV2_INSTALL_PATH}")
set (PLUGIN_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")



# Pseudo target for installing the test plugin in /usr/local/lib/lv2
add_custom_target(INSTALL_DOWNLOAD_TEST_PLUGIN
    COMMAND  cmake -E echo "Installing to ${PLUGIN_INSTALL_DIR}"
    COMMAND  cmake -E make_directory "${PLUGIN_INSTALL_DIR}"
    COMMAND  cmake -E make_directory "${PLUGIN_INSTALL_DIR}/resources"
    COMMAND  cmake -E copy "${PLUGIN_BINARY_DIR}/${LV2_SO_NAME}.so" "${PLUGIN_INSTALL_DIR}"
    COMMAND  cmake -E copy "${PLUGIN_BINARY_DIR}/${LV2_SO_NAME}_ui.so" "${PLUGIN_INSTALL_DIR}"
    COMMAND  cmake -E copy "${PLUGIN_BINARY_DIR}/manifest.ttl" "${PLUGIN_INSTALL_DIR}"
    COMMAND  cmake -E copy "${PLUGIN_BINARY_DIR}/${LV2_SO_NAME}.ttl" "${PLUGIN_INSTALL_DIR}"
    COMMAND  cmake -E copy_directory "${PROJECT_SOURCE_DIR}/resources" "${PLUGIN_INSTALL_DIR}/resources"
)

add_custom_command(
    OUTPUT  SamplePluginInfo.hpp          # Treated as relative to CMAKE_CURRENT_BINARY_DIR
    COMMAND generate_lv2c_plugin_info 
            ${PLUGIN_URI}
              --ttl ${CMAKE_CURRENT_BINARY_DIR}/
              --out ${CMAKE_CURRENT_BINARY_DIR}/SamplePluginInfo.hpp 
              --class SamplePluginInfo
    DEPENDS 
        ${CMAKE_CURRENT_BINARY_DIR}/manifest.ttl 
        ${CMAKE_CURRENT_BINARY_DIR}/${LV2_SO_NAME}.ttl    
        generate_lv2c_plugin_info
)

add_library(lv2tk_download_test SHARED

    SamplePlugin.cpp SamplePlugin.hpp
    Lv2Plugin.cpp
    Lv2Plugin.hpp
)
set_target_properties(lv2tk_download_test PROPERTIES OUTPUT_NAME "${LV2_SO_NAME}")
set_target_properties(lv2tk_download_test PROPERTIES PREFIX "")

target_compile_definitions(
    lv2tk_download_test PRIVATE
    PLUGIN_URI="${PLUGIN_URI}"
)

target_include_directories(
    lv2tk_download_test PRIVATE
)

target_link_directories(
    lv2tk_download_test PRIVATE
)

add_library(lv2tk_download_test_ui SHARED
    $<TARGET_OBJECTS:lv2c> $<TARGET_OBJECTS:lv2c_ui>

    SamplePluginUi.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/SamplePluginInfo.hpp 
)
set_target_properties(lv2tk_download_test_ui PROPERTIES OUTPUT_NAME "${LV2_UI_SO_NAME}")
set_target_properties(lv2tk_download_test_ui PROPERTIES PREFIX "")

target_link_options(lv2tk_download_test_ui PRIVATE
    "-Wl,-z,nodelete" 
    -Wl,-uiLinkTarget
)


target_compile_definitions(
    lv2tk_download_test_ui PRIVATE
    PLUGIN_URI="${PLUGIN_URI}"
    PLUGIN_UI_URI="${PLUGIN_UI_URI}"
)

target_include_directories(lv2tk_download_test_ui PRIVATE 
    ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(lv2tk_download_test_ui lv2c lv2c_ui)


option(INSTALL_LV2CAIRO_TEST_PLUGIN "Install LV2Cairo test Plugin" ON)

if (INSTALL_LV2CAIRO_TEST_PLUGIN)

    set(INSTALL_DIR /usr/lib/lv2/${LV2_INSTALL_PATH})

    install(TARGETS lv2tk_test lv2tk_download_test_ui
        LIBRARY DESTINATION ${INSTALL_DIR}
        )


    # Copy all assets to resources file
    INSTALL(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/manifest.ttl 
    ${CMAKE_CURRENT_BINARY_DIR}/${LV2_SO_NAME}.ttl
        DESTINATION ${INSTALL_DIR})

    # copy all resourece files to install resource directory.

    INSTALL(DIRECTORY ${PROJECT_SOURCE_DIR}/resources DESTINATION ${INSTALL_DIR} )
endif()
