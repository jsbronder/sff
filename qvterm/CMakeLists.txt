add_library(qvterm SHARED
    scrollback.cpp
    qvterm.cpp)
target_link_libraries(qvterm libvterm::libvterm Qt5::Widgets util)
target_include_directories(qvterm PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
