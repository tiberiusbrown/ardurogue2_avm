cmake_minimum_required(VERSION 3.20)

project(ardurogue2 LANGUAGES NONE)

option(AVM_DIG_RAY_COMPILER_REPRO
       "Build the original digging loop to reproduce an AVM backend crash" OFF)
if(AVM_DIG_RAY_COMPILER_REPRO)
    set(AVM_REPRO_DEFINE -DAVM_DIG_RAY_COMPILER_REPRO)
endif()

if(NOT DEFINED AVM_SDK_ROOT)
    set(AVM_SDK_ROOT "" CACHE PATH "Path to the installed AVM SDK")
endif()

if(CMAKE_HOST_WIN32)
    set(AVM_HOST_EXE_SUFFIX ".exe")
else()
    set(AVM_HOST_EXE_SUFFIX "")
endif()

if(AVM_SDK_ROOT)
    set(AVM_SDK_BIN "${AVM_SDK_ROOT}/bin")
    set(AVM_CLANG "${AVM_SDK_BIN}/avm-clang${AVM_HOST_EXE_SUFFIX}")
    set(AVM_LD "${AVM_SDK_BIN}/avm-ld${AVM_HOST_EXE_SUFFIX}")
    set(AVM_IMAGE "${AVM_SDK_BIN}/avm-image${AVM_HOST_EXE_SUFFIX}")
else()
    find_program(AVM_CLANG NAMES avm-clang)
    if(NOT AVM_CLANG)
        message(FATAL_ERROR
            "avm-clang was not found on PATH; set AVM_SDK_ROOT or add the AVM SDK bin directory to PATH")
    endif()
    file(REAL_PATH "${AVM_CLANG}" AVM_CLANG_REAL)
    get_filename_component(AVM_SDK_BIN "${AVM_CLANG_REAL}" DIRECTORY)
    get_filename_component(AVM_SDK_ROOT "${AVM_SDK_BIN}/.." ABSOLUTE)
    find_program(AVM_LD NAMES avm-ld HINTS "${AVM_SDK_BIN}")
    find_program(AVM_IMAGE NAMES avm-image HINTS "${AVM_SDK_BIN}")
    if(NOT AVM_LD OR NOT AVM_IMAGE)
        message(FATAL_ERROR
            "avm-ld and avm-image must be on PATH or in ${AVM_SDK_BIN}")
    endif()
endif()

set(AVM_SYSROOT "${AVM_SDK_ROOT}/sysroot")
set(AVM_INTERPRETER "${AVM_SDK_BIN}/avm/interp.hex")

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    foreach(AVM_REQUIRED_FILE IN ITEMS
            "${AVM_CLANG}" "${AVM_LD}" "${AVM_IMAGE}"
            "${AVM_INTERPRETER}"
            "${AVM_SYSROOT}/include/avm.h"
            "${AVM_SYSROOT}/lib/crt0.o"
            "${AVM_SYSROOT}/lib/libavm.a"
            "${AVM_SYSROOT}/lib/libavm-builtins.a")
        if(NOT EXISTS "${AVM_REQUIRED_FILE}")
            message(FATAL_ERROR "AVM SDK file not found: ${AVM_REQUIRED_FILE}")
        endif()
    endforeach()
endif()

set(ARDUROGUE2_OBJECTS)
set(ARDUROGUE2_ELF "${CMAKE_CURRENT_BINARY_DIR}/ardurogue2.elf")
set(ARDUROGUE2_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/ardurogue2.arduboy")
if(AVM_DIG_RAY_COMPILER_REPRO)
    set(ARDUROGUE2_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/ardurogue2-repro.arduboy")
endif()

set(ARDUROGUE2_SOURCES
    main state combat items world persistence status render ui)
set(ARDUROGUE2_HEADERS
    model game game_internal world app_state persistence status render ui
    inventory_view)
set(ARDUROGUE2_HEADER_FILES)
foreach(ARDUROGUE2_HEADER IN LISTS ARDUROGUE2_HEADERS)
    list(APPEND ARDUROGUE2_HEADER_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/${ARDUROGUE2_HEADER}.hpp")
endforeach()

foreach(ARDUROGUE2_SOURCE IN LISTS ARDUROGUE2_SOURCES)
    set(ARDUROGUE2_OBJECT "${CMAKE_CURRENT_BINARY_DIR}/${ARDUROGUE2_SOURCE}.o")
    list(APPEND ARDUROGUE2_OBJECTS "${ARDUROGUE2_OBJECT}")
    add_custom_command(
        OUTPUT "${ARDUROGUE2_OBJECT}"
        COMMAND "${AVM_CLANG}"
            --target=avm-unknown-arduboyfx
            -g -std=c++17 -O2 -flto -ffreestanding -fomit-frame-pointer
            -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables
            -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit
            -ffunction-sections -fdata-sections -Wall -Wextra -Werror
            ${AVM_REPRO_DEFINE}
            -isystem "${AVM_SYSROOT}/include"
            -c "${CMAKE_CURRENT_SOURCE_DIR}/src/${ARDUROGUE2_SOURCE}.cpp"
            -o "${ARDUROGUE2_OBJECT}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/${ARDUROGUE2_SOURCE}.cpp"
            ${ARDUROGUE2_HEADER_FILES}
        COMMENT "Compiling ${ARDUROGUE2_SOURCE}.cpp with the AVM SDK"
        VERBATIM
    )
endforeach()

add_custom_command(
    OUTPUT "${ARDUROGUE2_ELF}" "${ARDUROGUE2_IMAGE}"
    COMMAND "${AVM_LD}" -flavor gnu --entry=_start --gc-sections --lto-O2
        -o "${ARDUROGUE2_ELF}"
        "${AVM_SYSROOT}/lib/crt0.o"
        ${ARDUROGUE2_OBJECTS}
        "${AVM_SYSROOT}/lib/libavm.a"
        "${AVM_SYSROOT}/lib/libavm-builtins.a"
    COMMAND "${AVM_IMAGE}" "${ARDUROGUE2_ELF}"
        --title "ArduRogue 2"
        --description "Turn-based dungeon crawl for Arduboy FX"
        -o "${ARDUROGUE2_IMAGE}"
    DEPENDS ${ARDUROGUE2_OBJECTS}
        "${AVM_LD}"
        "${AVM_IMAGE}"
        "${AVM_INTERPRETER}"
        "${AVM_SYSROOT}/lib/crt0.o"
        "${AVM_SYSROOT}/lib/libavm.a"
        "${AVM_SYSROOT}/lib/libavm-builtins.a"
    COMMENT "Linking and packaging ardurogue2"
    VERBATIM
)

add_custom_target(ardurogue2 DEPENDS "${ARDUROGUE2_IMAGE}")
