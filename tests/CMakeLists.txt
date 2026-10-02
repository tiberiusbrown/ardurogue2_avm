cmake_minimum_required(VERSION 3.20)

project(ardurogue2_native_tests LANGUAGES CXX)

enable_testing()

add_executable(game_native
    game_native.cpp
    ../src/state.cpp
    ../src/combat.cpp
    ../src/items.cpp
    ../src/world.cpp
    ../src/persistence.cpp
)
target_compile_features(game_native PRIVATE cxx_std_17)
target_include_directories(game_native PRIVATE ../src)

add_test(NAME game_native COMMAND game_native)

add_executable(format_native format_native.cpp ../src/status.cpp)
target_compile_features(format_native PRIVATE cxx_std_17)
target_include_directories(format_native PRIVATE shim ../src)
add_test(NAME format_native COMMAND format_native)

add_executable(ui_native
    ui_native.cpp
    ../src/ui.cpp
    ../src/state.cpp
    ../src/combat.cpp
    ../src/items.cpp
    ../src/world.cpp
)
target_compile_features(ui_native PRIVATE cxx_std_17)
target_include_directories(ui_native PRIVATE shim ../src)
add_test(NAME ui_native COMMAND ui_native)

add_executable(render_native
    render_native.cpp
    ../src/render.cpp
    ../src/status.cpp
    ../src/state.cpp
    ../src/combat.cpp
    ../src/items.cpp
    ../src/world.cpp
)
target_compile_features(render_native PRIVATE cxx_std_17)
target_include_directories(render_native PRIVATE shim ../src)
add_test(NAME render_native COMMAND render_native)
