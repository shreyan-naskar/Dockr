cmake_minimum_required(VERSION 3.16)

project(Dockr LANGUAGES CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "Dockr requires Linux.")
endif()

add_executable(dockr
    src/main.cpp
    src/container.cpp
    src/network.cpp
    src/cgroup.cpp
    src/filesystem.cpp
    src/namespace_process.cpp
    src/clone_stack.cpp
    src/error.cpp
    src/config.cpp
)

target_compile_features(dockr PRIVATE cxx_std_17)

target_include_directories(dockr
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(dockr PRIVATE _GNU_SOURCE)

