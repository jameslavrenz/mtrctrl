#include "app.hpp"

#include "stm32f4xx_hal.h"

int main() {
    HAL_Init();
    led_init();
    if (!clock_to_180mhz()) {
        board_panic("clock");
    }
    uart_hw_init();
    if (!mtr::create_tasks()) {
        board_panic("tasks");
    }
    vTaskStartScheduler();
    board_panic("scheduler");
    return 0;
}
