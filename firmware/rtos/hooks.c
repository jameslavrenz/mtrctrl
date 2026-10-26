#include "FreeRTOS.h"
#include "task.h"

void board_panic(const char* reason);

volatile const char* g_panic_reason = "none";
volatile const char* g_assert_file;
volatile int g_assert_line;
volatile const char* g_overflow_task;

void vAssertCalled(const char* file, int line) {
    g_assert_file = file;
    g_assert_line = line;
    board_panic("assert");
}

void vApplicationStackOverflowHook(TaskHandle_t task, char* name) {
    (void)task;
    g_overflow_task = name;
    board_panic("stack");
}

static StaticTask_t g_idle_tcb;
static StackType_t g_idle_stack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory(StaticTask_t** tcb, StackType_t** stack, configSTACK_DEPTH_TYPE* depth) {
    *tcb = &g_idle_tcb;
    *stack = g_idle_stack;
    *depth = configMINIMAL_STACK_SIZE;
}
