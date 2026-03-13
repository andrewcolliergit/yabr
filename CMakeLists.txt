cmake_minimum_required(VERSION 3.20)
project(yabr VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SOURCES src/*.cpp)
add_executable(yabr ${SOURCES})
target_include_directories(yabr PRIVATE include)

if(MSVC)
    target_compile_options(yabr PRIVATE /W4)
else()
    target_compile_options(yabr PRIVATE -Wall -Wextra)
endif()

set_target_properties(yabr PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}
)
