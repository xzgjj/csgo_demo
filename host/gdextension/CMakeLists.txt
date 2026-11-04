cmake_minimum_required(VERSION 3.25)
project(recoil_host LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(GODOTCPP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/godot-cpp" CACHE PATH "")
set(GODOTCPP_LIB "${GODOTCPP_ROOT}/build/bin/libgodot-cpp.windows.template_debug.x86_64.a")

if(NOT EXISTS "${GODOTCPP_LIB}")
    message(FATAL_ERROR "godot-cpp 未构建：先运行 tools/fetch_godot_cpp.ps1（MinGW, template_debug）")
endif()

add_library(recoil_host SHARED recoil_host.cpp)

target_include_directories(recoil_host PRIVATE
    "${GODOTCPP_ROOT}/include"
    "${GODOTCPP_ROOT}/build/gen/include"
    "${GODOTCPP_ROOT}/gdextension"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../core/include"
)

target_link_libraries(recoil_host PRIVATE "${GODOTCPP_LIB}" "${CMAKE_CURRENT_SOURCE_DIR}/../../build/librecoil_core.a")

target_compile_definitions(recoil_host PRIVATE GDEXTENSION HOT_RELOAD_ENABLED=0)
target_compile_options(recoil_host PRIVATE -Wall -Wextra -Wpedantic)

set_target_properties(recoil_host PROPERTIES
    PREFIX "lib"
    OUTPUT_NAME "recoil_host"
    SUFFIX ".windows.template_debug.x86_64.dll"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../bin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../bin"
)
