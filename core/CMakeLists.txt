cmake_minimum_required(VERSION 3.25)
project(recoil_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(recoil_core STATIC
    src/bomb.cpp
    src/bot.cpp
    src/content.cpp
    src/grenades.cpp
    src/movement.cpp
    src/round.cpp
    src/step.cpp
)
target_include_directories(recoil_core PUBLIC include)
target_compile_options(recoil_core PRIVATE
    -Wall -Wextra -Wpedantic
    -fno-exceptions -fno-rtti
    -Wconversion -Wshadow
)

add_executable(recoil_tests
    tests/doctest_main.cpp
    tests/test_bomb.cpp
    tests/test_bot.cpp
    tests/test_grenades.cpp
    tests/test_helmet.cpp
    tests/test_hitgroups.cpp
    tests/test_movement.cpp
    tests/test_round.cpp
    tests/test_step.cpp
)
target_include_directories(recoil_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../third_party)
target_link_libraries(recoil_tests PRIVATE recoil_core)

enable_testing()
add_test(NAME core_tests COMMAND recoil_tests)
