#include "platform.hpp"

#include "mtr/units.hpp"
#include "timer_split.hpp"

#include "stm32f4xx_hal.h"

namespace {

TIM_HandleTypeDef g_tim7{};
TaskHandle_t g_control_task = nullptr;
uint32_t g_expected_cycles = 0;
uint32_t g_previous_isr = 0;
volatile uint32_t g_last_isr = 0;
volatile int32_t g_jitter_cycles = 0;
volatile uint32_t g_jitter_valid = 0;
bool g_have_previous = false;

}  // namespace

extern "C" void control_release_from_isr() {
    const uint32_t now = DWT->CYCCNT;
    if (g_have_previous) {
        const auto interval = static_cast<int32_t>(now - g_previous_isr);
        g_jitter_cycles = interval - static_cast<int32_t>(g_expected_cycles);
        g_jitter_valid = 1u;
    }
    g_have_previous = true;
    g_previous_isr = now;
    g_last_isr = now;

    BaseType_t higher_priority_woken = pdFALSE;
    if (g_control_task != nullptr) {
        vTaskNotifyGiveFromISR(g_control_task, &higher_priority_woken);
        portYIELD_FROM_ISR(higher_priority_woken);
    }
}

void control_clock_start(TaskHandle_t task) {
    g_control_task = task;
    g_expected_cycles = SystemCoreClock / mtr::kControlHz;
    const uint32_t timer_clock = apb1_timer_clock_hz();
    if (timer_clock < mtr::kControlHz || (timer_clock % mtr::kControlHz) != 0u) {
        board_panic("control timer");
    }
    uint32_t prescaler = 0;
    uint32_t autoreload = 0;
    if (!split_timer_product(timer_clock / mtr::kControlHz, prescaler, autoreload)) {
        board_panic("control timer");
    }

    __HAL_RCC_TIM7_CLK_ENABLE();
    g_tim7.Instance = TIM7;
    g_tim7.Init.Prescaler = prescaler;
    g_tim7.Init.CounterMode = TIM_COUNTERMODE_UP;
    g_tim7.Init.Period = autoreload;
    g_tim7.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    g_tim7.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&g_tim7) != HAL_OK) {
        board_panic("control timer");
    }
    // Priority 6 is at or below the syscall ceiling, so FromISR is legal, and
    // it is still more urgent than the USART IRQ.
    HAL_NVIC_SetPriority(TIM7_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(TIM7_IRQn);
    if (HAL_TIM_Base_Start_IT(&g_tim7) != HAL_OK) {
        board_panic("control timer");
    }
}

void control_clock_stop() { (void)HAL_TIM_Base_Stop_IT(&g_tim7); }

uint32_t control_clock_last_isr() { return g_last_isr; }

int32_t control_clock_last_jitter() { return g_jitter_cycles; }

bool control_clock_jitter_valid() { return g_jitter_valid != 0u; }

extern "C" void TIM7_IRQHandler(void) { HAL_TIM_IRQHandler(&g_tim7); }
