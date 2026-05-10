cmake_minimum_required(VERSION 3.25)
project(Hyperion VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/W4 /MP /permissive-)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(imgui CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(zydis CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(ZLIB REQUIRED)

file(GLOB_RECURSE HYPERION_SOURCES
    "src/*.cpp"
    "src/*.h"
)

add_executable(${PROJECT_NAME} ${HYPERION_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    imgui::imgui
    glfw
    OpenGL::GL
    Zydis::Zydis
    spdlog::spdlog
    fmt::fmt
    ZLIB::ZLIB
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE comdlg32)
endif()

target_precompile_headers(${PROJECT_NAME} PRIVATE
    <cstdint>
    <string>
    <vector>
    <unordered_map>
    <memory>
    <algorithm>
    <thread>
    <mutex>
    <atomic>
    <functional>
    <span>
)
