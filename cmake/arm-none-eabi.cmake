set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_CROSSCOMPILING TRUE)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

execute_process(
    COMMAND sh "${CMAKE_CURRENT_LIST_DIR}/../tools/find-arm-cxx.sh"
    OUTPUT_VARIABLE MTR_ARM_CXX
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE MTR_ARM_CXX_RESULT
)
if(NOT MTR_ARM_CXX_RESULT EQUAL 0)
    message(FATAL_ERROR "Arm C++ toolchain not found. Set ARM_CXX to arm-none-eabi-g++.")
endif()

get_filename_component(MTR_ARM_BIN "${MTR_ARM_CXX}" DIRECTORY)
set(CMAKE_C_COMPILER "${MTR_ARM_BIN}/arm-none-eabi-gcc")
set(CMAKE_CXX_COMPILER "${MTR_ARM_CXX}")
set(CMAKE_ASM_COMPILER "${MTR_ARM_BIN}/arm-none-eabi-gcc")

set(MTR_CPU_FLAGS "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -ffunction-sections -fdata-sections")
set(CMAKE_C_FLAGS_INIT "${MTR_CPU_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${MTR_CPU_FLAGS} -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit")
set(CMAKE_ASM_FLAGS_INIT "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
