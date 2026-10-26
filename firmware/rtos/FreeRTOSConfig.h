#pragma once

#include <stdint.h>

// Native FreeRTOS on STM32F446, SYSCLK 180 MHz.
// SysTick belongs to the kernel. The HAL time base is TIM6 and must not call
// FreeRTOS. USE_RTOS in the HAL config stays 0: that switch is the old
// CMSIS-RTOS hook, which this project does not use.

#define configUSE_PREEMPTION 1
#define configUSE_TIME_SLICING 0
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#define configUSE_TICKLESS_IDLE 0
#define configCPU_CLOCK_HZ 180000000UL
#define configTICK_RATE_HZ 1000UL
#define configMAX_PRIORITIES 6
#define configMINIMAL_STACK_SIZE 256
#define configMAX_TASK_NAME_LEN 12
#define configUSE_16_BIT_TICKS 0
#define configIDLE_SHOULD_YIELD 1
#define configUSE_TASK_NOTIFICATIONS 1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES 1
#define configUSE_MUTEXES 1
#define configUSE_RECURSIVE_MUTEXES 0
#define configUSE_COUNTING_SEMAPHORES 0
#define configUSE_QUEUE_SETS 0
#define configUSE_TIMERS 0
#define configUSE_CO_ROUTINES 0
#define configUSE_IDLE_HOOK 0
#define configUSE_TICK_HOOK 0
#define configUSE_MALLOC_FAILED_HOOK 0
#define configCHECK_FOR_STACK_OVERFLOW 2
#define configSUPPORT_STATIC_ALLOCATION 1
#define configSUPPORT_DYNAMIC_ALLOCATION 0
#define configAPPLICATION_ALLOCATED_HEAP 0
#define configUSE_NEWLIB_REENTRANT 0
#define configENABLE_FPU 1
#define configENABLE_MPU 0
#define configENABLE_TRUSTZONE 0

#define configUSE_TRACE_FACILITY 1
#define configGENERATE_RUN_TIME_STATS 1
#define configUSE_STATS_FORMATTING_FUNCTIONS 0
#define configRUN_TIME_COUNTER_TYPE uint32_t

// DWT cycle counter. At 180 MHz it wraps about every 23.8 s. Windowed
// differences shorter than that are valid. The counter is shared with the
// control-loop timing measurements.
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()                          \
    do {                                                                  \
        *(volatile uint32_t*)0xE000EDFCUL |= (1UL << 24);                 \
        *(volatile uint32_t*)0xE0001004UL = 0;                            \
        *(volatile uint32_t*)0xE0001000UL |= 1UL;                         \
    } while (0)
#define portGET_RUN_TIME_COUNTER_VALUE() (*(volatile uint32_t*)0xE0001004UL)

#define configPRIO_BITS 4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#define configKERNEL_INTERRUPT_PRIORITY (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

#define configASSERT(expression)                            \
    do {                                                    \
        if ((expression) == 0) {                            \
            vAssertCalled(__FILE__, __LINE__);              \
        }                                                   \
    } while (0)

#ifdef __cplusplus
extern "C" {
#endif
void vAssertCalled(const char* file, int line);
#ifdef __cplusplus
}
#endif

#define INCLUDE_vTaskDelay 1
/* vTaskDelayUntil() remains as a macro over xTaskDelayUntil(). This kernel
 * rejects defining both INCLUDE_ macros. */
#define INCLUDE_xTaskDelayUntil 1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define INCLUDE_vTaskPrioritySet 1
#define INCLUDE_vTaskSuspend 0
#define INCLUDE_xTaskGetSchedulerState 1
#define INCLUDE_xTaskGetCurrentTaskHandle 1
#define INCLUDE_xTaskGetIdleTaskHandle 1

#define vPortSVCHandler SVC_Handler
#define xPortPendSVHandler PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

#define configENFORCE_SYSTEM_CALLS_FROM_KERNEL_ONLY 0
