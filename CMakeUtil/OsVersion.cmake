function(get_linux_lsb_release_information)
    find_program(LSB_RELEASE_EXEC lsb_release)
    if(NOT LSB_RELEASE_EXEC)
        message(FATAL_ERROR "Could not detect lsb_release executable, can not gather required information")
    endif()

    execute_process(COMMAND "${LSB_RELEASE_EXEC}" --short --id OUTPUT_VARIABLE OSV_LINUX_DISTRO OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND "${LSB_RELEASE_EXEC}" --short --release OUTPUT_VARIABLE OSV_LINUX_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND "${LSB_RELEASE_EXEC}" --short --codename OUTPUT_VARIABLE OSV_LINUX_CODENAME OUTPUT_STRIP_TRAILING_WHITESPACE)

    set(OSV_LINUX_DISTRO "${OSV_LINUX_DISTRO}" PARENT_SCOPE)
    set(OSV_LINUX_VERSION "${OSV_LINUX_VERSION}" PARENT_SCOPE)
    set(OSV_LINUX_CODENAME "${OSV_LINUX_CODENAME}" PARENT_SCOPE)
endfunction()

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    get_linux_lsb_release_information()
endif()

message(STATUS "PLATFORM: ${CMAKE_SYSTEM_NAME} ${OSV_LINUX_DISTRO} ${OSV_LINUX_VERSION} ${OSV_LINUX_CODENAME}")
