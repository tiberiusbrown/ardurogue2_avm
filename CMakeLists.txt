cmake_minimum_required(VERSION 3.20)

project(ardurogue2 LANGUAGES NONE)

if(NOT DEFINED AVM_SDK_ROOT)
    set(AVM_SDK_ROOT "" CACHE PATH "Path to the installed AVM SDK")
endif()
if(NOT AVM_SDK_ROOT)
    message(FATAL_ERROR "Set AVM_SDK_ROOT to the installed AVM SDK directory")
endif()

if(CMAKE_HOST_WIN32)
    set(AVM_HOST_EXE_SUFFIX ".exe")
else()
    set(AVM_HOST_EXE_SUFFIX "")
endif()

set(AVM_SDK_BIN "${AVM_SDK_ROOT}/bin")
set(AVM_SYSROOT "${AVM_SDK_ROOT}/sysroot")
set(AVM_CLANG "${AVM_SDK_BIN}/avm-clang${AVM_HOST_EXE_SUFFIX}")
set(AVM_LD "${AVM_SDK_BIN}/avm-ld${AVM_HOST_EXE_SUFFIX}")
set(AVM_IMAGE "${AVM_SDK_BIN}/avm-image${AVM_HOST_EXE_SUFFIX}")

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR
        AND NOT EXISTS "${AVM_CLANG}")
    message(FATAL_ERROR "AVM SDK not found at ${AVM_SDK_ROOT}; build and install it first")
endif()

set(ARDUROGUE2_OBJECT "${CMAKE_CURRENT_BINARY_DIR}/main.o")
set(ARDUROGUE2_ELF "${CMAKE_CURRENT_BINARY_DIR}/ardurogue2.elf")
set(ARDUROGUE2_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/ardurogue2.bin")

add_custom_command(
    OUTPUT "${ARDUROGUE2_OBJECT}"
    COMMAND "${AVM_CLANG}"
        --target=avm-unknown-arduboyfx
        -std=c++17 -O2 -ffreestanding -fomit-frame-pointer
        -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables
        -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit
        -ffunction-sections -fdata-sections -Wall -Wextra -Werror
        -isystem "${AVM_SYSROOT}/include"
        -c "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
        -o "${ARDUROGUE2_OBJECT}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
    COMMENT "Compiling ardurogue2 with the AVM SDK"
    VERBATIM
)

add_custom_command(
    OUTPUT "${ARDUROGUE2_ELF}" "${ARDUROGUE2_IMAGE}"
    COMMAND "${AVM_LD}" -flavor gnu --entry=_start --gc-sections
        -o "${ARDUROGUE2_ELF}"
        "${AVM_SYSROOT}/lib/crt0.o"
        "${ARDUROGUE2_OBJECT}"
        "${AVM_SYSROOT}/lib/libavm.a"
        "${AVM_SYSROOT}/lib/libavm-builtins.a"
    COMMAND "${AVM_IMAGE}" --development "${ARDUROGUE2_ELF}"
        -o "${ARDUROGUE2_IMAGE}"
    DEPENDS "${ARDUROGUE2_OBJECT}"
    COMMENT "Linking and packaging ardurogue2"
    VERBATIM
)

add_custom_target(ardurogue2 DEPENDS "${ARDUROGUE2_IMAGE}")
