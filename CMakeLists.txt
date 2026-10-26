cmake_minimum_required(VERSION 3.20)

project(mtrctrl LANGUAGES C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(mtr_logic STATIC
    src/mtr/pid.cpp
    src/mtr/simulated_motor.cpp
    src/mtr/fault_policy.cpp
    src/mtr/control_loop.cpp
    src/mtr/command.cpp
    src/mtr/snapshot.cpp
    src/mtr/text.cpp
    src/mtr/telemetry_row.cpp
)
target_include_directories(mtr_logic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(mtr_logic PRIVATE
    -Wall -Wextra -Wpedantic -Wshadow -Wundef -Wdouble-promotion -Wformat=2
    -Werror -fno-exceptions -fno-rtti
)

if(CMAKE_CROSSCOMPILING)
    add_subdirectory(firmware)
else()
    enable_testing()
    add_executable(mtr_unit_tests
        tests/test_main.cpp
        tests/test_runner.cpp
        tests/test_pid.cpp
        tests/test_motor.cpp
        tests/test_loop.cpp
        tests/test_support.cpp
        tests/test_telemetry.cpp
    )
    target_include_directories(mtr_unit_tests PRIVATE tests)
    target_compile_definitions(mtr_unit_tests PRIVATE "MTR_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\"")
    target_link_libraries(mtr_unit_tests PRIVATE mtr_logic)
    # -fno-rtti matches mtr_logic. GCC otherwise asks the library for
    # SimulatedMotor's typeinfo, which the library does not emit.
    target_compile_options(mtr_unit_tests PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow -Werror
        -fno-rtti
        -fsanitize=address,undefined -fno-omit-frame-pointer
    )
    target_link_options(mtr_unit_tests PRIVATE -fsanitize=address,undefined)
    add_test(NAME unit COMMAND mtr_unit_tests)
    add_test(NAME telemetry_columns COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/check_telemetry_columns.py)
endif()
