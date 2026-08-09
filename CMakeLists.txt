cmake_minimum_required(VERSION 3.16)

project(Dockr LANGUAGES CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "Dockr requires Linux.")
endif()

find_package(Threads REQUIRED)

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

add_executable(client
    demo/client.cpp
)

target_compile_features(client PRIVATE cxx_std_17)

add_executable(server
    demo/server.cpp
)

target_compile_features(server PRIVATE cxx_std_17)

set_target_properties(server
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/shared_folder
)

add_executable(hungry
    demo/hungry.hpp
)

set_source_files_properties(demo/hungry.hpp
    PROPERTIES
        LANGUAGE CXX
        COMPILE_FLAGS "-x c++"
)

target_compile_features(hungry PRIVATE cxx_std_17)

target_link_libraries(hungry PRIVATE Threads::Threads)

set_target_properties(hungry
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/shared_folder
)
