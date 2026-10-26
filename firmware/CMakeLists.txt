add_executable(mtrctrl
    app/main.cpp
    app/app.cpp
    app/control_task.cpp
    app/command_task.cpp
    app/telemetry_task.cpp
    app/supervisor_task.cpp
    platform/board.cpp
    platform/clock.cpp
    platform/timebase.cpp
    platform/control_clock.cpp
    platform/uart.cpp
    platform/syscalls.c
    platform/startup_stm32f446xx.s
    rtos/hooks.c
    ${CMAKE_SOURCE_DIR}/third_party/FreeRTOS-Kernel/tasks.c
    ${CMAKE_SOURCE_DIR}/third_party/FreeRTOS-Kernel/queue.c
    ${CMAKE_SOURCE_DIR}/third_party/FreeRTOS-Kernel/list.c
    ${CMAKE_SOURCE_DIR}/third_party/FreeRTOS-Kernel/portable/GCC/ARM_CM4F/port.c
    ${CMAKE_SOURCE_DIR}/third_party/cmsis-device-f4/Source/system_stm32f4xx.c
    ${CMAKE_SOURCE_DIR}/third_party/stm32f4xx-hal/Src/stm32f4xx_hal.c
    ${CMAKE_SOURCE_DIR}/third_party/stm32f4xx-hal/Src/stm32f4xx_hal_cortex.c
    ${CMAKE_SOURCE_DIR}/third_party/stm32f4xx-hal/Src/stm32f4xx_hal_rcc.c
    ${CMAKE_SOURCE_DIR}/third_party/stm32f4xx-hal/Src/stm32f4xx_hal_rcc_ex.c
    ${CMAKE_SOURCE_DIR}/third_party/stm32f4xx-hal/Src/stm32f4xx_hal_gpio.c
    ${CMAKE_SOURCE_DIR}/third_party/stm32f4xx-hal/Src/stm32f4xx_hal_uart.c
    ${CMAKE_SOURCE_DIR}/third_party/stm32f4xx-hal/Src/stm32f4xx_hal_dma.c
    ${CMAKE_SOURCE_DIR}/third_party/stm32f4xx-hal/Src/stm32f4xx_hal_tim.c
    ${CMAKE_SOURCE_DIR}/third_party/stm32f4xx-hal/Src/stm32f4xx_hal_tim_ex.c
    ${CMAKE_SOURCE_DIR}/third_party/stm32f4xx-hal/Src/stm32f4xx_hal_pwr.c
    ${CMAKE_SOURCE_DIR}/third_party/stm32f4xx-hal/Src/stm32f4xx_hal_pwr_ex.c
    ${CMAKE_SOURCE_DIR}/third_party/stm32f4xx-hal/Src/stm32f4xx_hal_flash.c
    ${CMAKE_SOURCE_DIR}/third_party/stm32f4xx-hal/Src/stm32f4xx_hal_flash_ex.c
)

target_link_libraries(mtrctrl PRIVATE mtr_logic)

target_include_directories(mtrctrl PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/app
    ${CMAKE_CURRENT_SOURCE_DIR}/platform
    ${CMAKE_CURRENT_SOURCE_DIR}/rtos
    ${CMAKE_SOURCE_DIR}/third_party/stm32f4xx-hal/Inc
    ${CMAKE_SOURCE_DIR}/third_party/cmsis-device-f4/Include
    ${CMAKE_SOURCE_DIR}/third_party/cmsis/Include
    ${CMAKE_SOURCE_DIR}/third_party/FreeRTOS-Kernel/include
    ${CMAKE_SOURCE_DIR}/third_party/FreeRTOS-Kernel/portable/GCC/ARM_CM4F
)

target_compile_definitions(mtrctrl PRIVATE
    STM32F446xx
    HSE_VALUE=8000000U
)

target_compile_options(mtrctrl PRIVATE
    -Wall -Wextra -Wshadow -Wundef -Wno-unused-parameter
    $<$<COMPILE_LANGUAGE:CXX>:-Wpedantic -Wdouble-promotion -Wformat=2 -Wnon-virtual-dtor -Werror>
)

set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/third_party/FreeRTOS-Kernel/tasks.c
    ${CMAKE_SOURCE_DIR}/third_party/FreeRTOS-Kernel/queue.c
    ${CMAKE_SOURCE_DIR}/third_party/FreeRTOS-Kernel/list.c
    ${CMAKE_SOURCE_DIR}/third_party/FreeRTOS-Kernel/portable/GCC/ARM_CM4F/port.c
    ${CMAKE_SOURCE_DIR}/third_party/cmsis-device-f4/Source/system_stm32f4xx.c
    ${CMAKE_SOURCE_DIR}/third_party/stm32f4xx-hal/Src/stm32f4xx_hal.c
    ${CMAKE_SOURCE_DIR}/third_party/stm32f4xx-hal/Src/stm32f4xx_hal_cortex.c
    ${CMAKE_SOURCE_DIR}/third_party/stm32f4xx-hal/Src/stm32f4xx_hal_rcc.c
    ${CMAKE_SOURCE_DIR}/third_party/stm32f4xx-hal/Src/stm32f4xx_hal_rcc_ex.c
    ${CMAKE_SOURCE_DIR}/third_party/stm32f4xx-hal/Src/stm32f4xx_hal_gpio.c
    ${CMAKE_SOURCE_DIR}/third_party/stm32f4xx-hal/Src/stm32f4xx_hal_uart.c
    ${CMAKE_SOURCE_DIR}/third_party/stm32f4xx-hal/Src/stm32f4xx_hal_dma.c
    ${CMAKE_SOURCE_DIR}/third_party/stm32f4xx-hal/Src/stm32f4xx_hal_tim.c
    ${CMAKE_SOURCE_DIR}/third_party/stm32f4xx-hal/Src/stm32f4xx_hal_tim_ex.c
    ${CMAKE_SOURCE_DIR}/third_party/stm32f4xx-hal/Src/stm32f4xx_hal_pwr.c
    ${CMAKE_SOURCE_DIR}/third_party/stm32f4xx-hal/Src/stm32f4xx_hal_pwr_ex.c
    ${CMAKE_SOURCE_DIR}/third_party/stm32f4xx-hal/Src/stm32f4xx_hal_flash.c
    ${CMAKE_SOURCE_DIR}/third_party/stm32f4xx-hal/Src/stm32f4xx_hal_flash_ex.c
    PROPERTIES COMPILE_OPTIONS "-w"
)

target_link_options(mtrctrl PRIVATE
    -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
    -Wl,--gc-sections
    -Wl,-Map,$<TARGET_FILE_DIR:mtrctrl>/mtrctrl.map
    -Wl,--print-memory-usage
    -T${CMAKE_CURRENT_SOURCE_DIR}/linker/stm32f446re.ld
    --specs=nano.specs
    -nostartfiles
)

set_target_properties(mtrctrl PROPERTIES SUFFIX ".elf")

get_filename_component(MTR_ARM_BIN "${CMAKE_C_COMPILER}" DIRECTORY)
find_program(MTR_SIZE arm-none-eabi-size HINTS "${MTR_ARM_BIN}" REQUIRED)
find_program(MTR_OBJCOPY arm-none-eabi-objcopy HINTS "${MTR_ARM_BIN}" REQUIRED)

add_custom_command(TARGET mtrctrl POST_BUILD
    COMMAND ${MTR_SIZE} $<TARGET_FILE:mtrctrl>
    COMMAND ${MTR_OBJCOPY} -O ihex $<TARGET_FILE:mtrctrl> $<TARGET_FILE_DIR:mtrctrl>/mtrctrl.hex
    COMMAND ${MTR_OBJCOPY} -O binary $<TARGET_FILE:mtrctrl> $<TARGET_FILE_DIR:mtrctrl>/mtrctrl.bin
)
