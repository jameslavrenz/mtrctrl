#include "platform.hpp"

#include "timer_split.hpp"

#include "stm32f4xx_hal.h"

namespace {

TIM_HandleTypeDef g_tim6{};
bool g_tim6_ready = false;

}  // namespace

extern "C" void control_release_from_isr();

extern "C" HAL_StatusTypeDef HAL_InitTick(uint32_t tick_priority) {
    (void)tick_priority;
    __HAL_RCC_TIM6_CLK_ENABLE();
    const uint32_t timer_clock = apb1_timer_clock_hz();
    if (timer_clock < 1000u || (timer_clock % 1000u) != 0u) {
        return HAL_ERROR;
    }
    uint32_t prescaler = 0;
    uint32_t autoreload = 0;
    if (!split_timer_product(timer_clock / 1000u, prescaler, autoreload)) {
        return HAL_ERROR;
    }

    if (g_tim6_ready) {
        (void)HAL_TIM_Base_Stop_IT(&g_tim6);
        (void)HAL_TIM_Base_DeInit(&g_tim6);
    }

    g_tim6.Instance = TIM6;
    g_tim6.Init.Prescaler = prescaler;
    g_tim6.Init.CounterMode = TIM_COUNTERMODE_UP;
    g_tim6.Init.Period = autoreload;
    g_tim6.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    g_tim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&g_tim6) != HAL_OK) {
        return HAL_ERROR;
    }
    // Priority 2 is more urgent than configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY
    // (5), so this ISR must not call FreeRTOS. It only increments the HAL tick.
    HAL_NVIC_SetPriority(TIM6_DAC_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
    if (HAL_TIM_Base_Start_IT(&g_tim6) != HAL_OK) {
        return HAL_ERROR;
    }
    uwTickPrio = 2u;
    g_tim6_ready = true;
    return HAL_OK;
}

extern "C" void HAL_SuspendTick(void) { __HAL_TIM_DISABLE_IT(&g_tim6, TIM_IT_UPDATE); }

extern "C" void HAL_ResumeTick(void) { __HAL_TIM_ENABLE_IT(&g_tim6, TIM_IT_UPDATE); }

extern "C" void TIM6_DAC_IRQHandler(void) { HAL_TIM_IRQHandler(&g_tim6); }

extern "C" void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* timer) {
    if (timer->Instance == TIM6) {
        HAL_IncTick();
    } else if (timer->Instance == TIM7) {
        control_release_from_isr();
    }
}
