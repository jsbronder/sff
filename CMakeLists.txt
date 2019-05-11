cmake_minimum_required(VERSION 3.9.6 FATAL_ERROR)
project(sff VERSION 0.0.1 LANGUAGES CXX C)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE CACHE STRING "" FORCE)
endif()

set(CMAKE_CXX_STANDARD 14)
set(warning_flags "-Wall -Wconversion -Werror -Wextra -fPIC")
string(REPLACE " " ";" cxx_warning_flags "${warning_flags}")
add_compile_options(
    "$<$<COMPILE_LANGUAGE:CXX>:${cxx_warning_flags}>"
)

set(VENDOR_LIBVTERM YES CACHE BOOL "Use vendored libvterm")

if (VENDOR_LIBVTERM)
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/root/include)
    add_library(libvterm::libvterm SHARED IMPORTED)
    set_target_properties(libvterm::libvterm
        PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_BINARY_DIR}/root/include/
        IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/root/lib/libvterm.so)
    add_dependencies(libvterm::libvterm libvterm_vendored)
else()
    # This is speculative
    find_package(libvterm REQUIRED)
endif()

add_subdirectory(ext)
