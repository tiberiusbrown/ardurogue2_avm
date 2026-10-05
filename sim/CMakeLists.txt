add_library(ardurogue2_sim_core STATIC
    simulator.cpp omniscient_agent.cpp metrics.cpp trace.cpp frontend.cpp
    ../src/state.cpp ../src/combat.cpp ../src/combat_math.cpp ../src/items.cpp
    ../src/world.cpp ../src/world_gen.cpp ../src/world_gen_population.cpp)
target_compile_features(ardurogue2_sim_core PUBLIC cxx_std_17)
target_compile_definitions(ardurogue2_sim_core PUBLIC ARDUROGUE2_SIM)
target_include_directories(ardurogue2_sim_core PUBLIC . ../src)
if(MSVC)
    target_compile_options(ardurogue2_sim_core PRIVATE /W4)
else()
    target_compile_options(ardurogue2_sim_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_executable(ardurogue2_sim main.cpp)
target_link_libraries(ardurogue2_sim PRIVATE ardurogue2_sim_core)
add_executable(sim_checks tests.cpp)
target_link_libraries(sim_checks PRIVATE ardurogue2_sim_core)
add_test(NAME simulator_checks COMMAND sim_checks)
find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME simulator_cli COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_cli.py" "$<TARGET_FILE:ardurogue2_sim>")
endif()
