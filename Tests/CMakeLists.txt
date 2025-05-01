cmake_minimum_required(VERSION 3.8)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

set(This FileCollectorTests)

set(Sources
    FileCollectorUnitTest.cpp
    FileCollectorIntegrationTest.cpp
)

add_executable(${This} ${Sources})

target_include_directories(${This} PUBLIC
    ${CMAKE_SOURCE_DIR}/FileCollector
)

set_property(TARGET ${This} PROPERTY CXX_STANDARD 20)

target_link_libraries(${This} PUBLIC
    gtest_main
    FileCollector
)

add_test(
    NAME ${This}
    COMMAND ${This}
)
