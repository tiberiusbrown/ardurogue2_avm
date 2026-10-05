cmake_minimum_required(VERSION 3.20)

project(ardurogue2_native_tests LANGUAGES CXX)

enable_testing()

add_executable(game_native
    game_native.cpp
    combat_checks.cpp
    weapon_checks.cpp
    bench_checks.cpp
    ../bench/bench.cpp
    ../src/state.cpp
    ../src/combat.cpp
    ../src/combat_math.cpp
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
    ../src/combat_math.cpp
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
    ../src/combat_math.cpp
    ../src/items.cpp
    ../src/world.cpp
)
target_compile_features(render_native PRIVATE cxx_std_17)
target_include_directories(render_native PRIVATE shim ../src)
add_test(NAME render_native COMMAND render_native)

# Optional seeded balance diagnostic using the same production RNG/helpers.
add_test(NAME combat_distributions COMMAND game_native --combat-distributions)

find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME shared_visibility_generated
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/../tools/generate_ray_sight.py" --check)
    add_test(NAME turn_benchmark_helpers
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/../bench/test_profile_turns.py")
endif()
