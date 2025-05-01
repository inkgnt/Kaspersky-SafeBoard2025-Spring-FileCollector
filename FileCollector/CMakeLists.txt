cmake_minimum_required(VERSION 3.8)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

set(This FileCollector)

set(Headers
    FileCollector.hpp
)
set(Sources
    FileCollector.cpp
)

add_library(${This} STATIC ${Sources} ${Headers})

target_include_directories(${This} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set_property(TARGET ${This} PROPERTY CXX_STANDARD 20)
