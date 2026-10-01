cmake_minimum_required(VERSION 3.20)

project(ardurogue2_native_tests LANGUAGES CXX)

enable_testing()

add_executable(game_native
    game_native.cpp
    ../src/game.cpp
    ../src/world.cpp
)
target_compile_features(game_native PRIVATE cxx_std_17)
target_include_directories(game_native PRIVATE ../src)

add_test(NAME game_native COMMAND game_native)
