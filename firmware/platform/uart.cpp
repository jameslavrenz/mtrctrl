#include "platform.hpp"

#include "app.hpp"

#include "stm32f4xx_hal.h"

#include <atomic>
#include <cstring>

namespace {

constexpr uint16_t kRxBytes = 256;
constexpr uint16_t kTxBytes = 1024;

uint8_t g_rx[kRxBytes];
uint8_t g_tx[kTxBytes];
std::atomic<uint16_t> g_rx_head{0};
std::atomic<uint16_t> g_rx_tail{0};
std::atomic<uint16_t> g_tx_head{0};
std::atomic<uint16_t> g_tx_tail{0};
UART_HandleTypeDef g_uart{};

static_assert(std::atomic<uint16_t>::is_always_lock_free, "UART indices must be lock-free");

bool rx_push_from_isr(uint8_t byte) {
    const uint16_t head = g_rx_head.load(std::memory_order_relaxed);
    const auto next = static_cast<uint16_t>((head + 1u) & (kRxBytes - 1u));
    if (next == g_rx_tail.load(std::memory_order_acquire)) {
        mtr::app().rx_dropped.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    g_rx[head] = byte;
    g_rx_head.store(next, std::memory_order_release);
    return true;
}

bool tx_push(uint8_t byte) {
    const uint16_t head = g_tx_head.load(std::memory_order_relaxed);
    const auto next = static_cast<uint16_t>((head + 1u) & (kTxBytes - 1u));
    if (next == g_tx_tail.load(std::memory_order_acquire)) {
        return false;
    }
    g_tx[head] = byte;
    g_tx_head.store(next, std::memory_order_release);
    return true;
}

bool tx_pop_from_isr(uint8_t& byte) {
    const uint16_t tail = g_tx_tail.load(std::memory_order_relaxed);
    if (tail == g_tx_head.load(std::memory_order_acquire)) {
        return false;
    }
    byte = g_tx[tail];
    g_tx_tail.store(static_cast<uint16_t>((tail + 1u) & (kTxBytes - 1u)), std::memory_order_release);
    return true;
}

}  // namespace

extern "C" void HAL_UART_MspInit(UART_HandleTypeDef* uart) {
    if (uart->Instance != USART2) {
        return;
    }
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART2_CLK_ENABLE();
    GPIO_InitTypeDef gpio{};
    gpio.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &gpio);
    HAL_NVIC_SetPriority(USART2_IRQn, 7, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

void uart_hw_init() {
    g_uart.Instance = USART2;
    g_uart.Init.BaudRate = 115200;
    g_uart.Init.WordLength = UART_WORDLENGTH_8B;
    g_uart.Init.StopBits = UART_STOPBITS_1;
    g_uart.Init.Parity = UART_PARITY_NONE;
    g_uart.Init.Mode = UART_MODE_TX_RX;
    g_uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    g_uart.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&g_uart) != HAL_OK) {
        board_panic("uart");
    }
}

void uart_kick_tx() {
    taskENTER_CRITICAL();
    USART2->CR1 |= USART_CR1_TXEIE;
    taskEXIT_CRITICAL();
}

void uart_enable_rx() {
    // Before any transmit. The TX ISR clears TXEIE with a read-modify-write of
    // CR1; doing that while this bit is being set drops the receiver interrupt.
    taskENTER_CRITICAL();
    USART2->CR1 |= USART_CR1_RXNEIE;
    taskEXIT_CRITICAL();
}

bool uart_rx_pop(uint8_t& byte) {
    const uint16_t tail = g_rx_tail.load(std::memory_order_relaxed);
    if (tail == g_rx_head.load(std::memory_order_acquire)) {
        return false;
    }
    byte = g_rx[tail];
    g_rx_tail.store(static_cast<uint16_t>((tail + 1u) & (kRxBytes - 1u)), std::memory_order_release);
    return true;
}

void uart_write(const char* text) {
    if (text == nullptr) {
        return;
    }
    if (xSemaphoreTake(mtr::app().tx_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }
    for (const char* cursor = text; *cursor != '\0'; ++cursor) {
        while (!tx_push(static_cast<uint8_t>(*cursor))) {
            uart_kick_tx();
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
    uart_kick_tx();
    (void)xSemaphoreGive(mtr::app().tx_mutex);
}

extern "C" void USART2_IRQHandler(void) {
    const uint32_t status = USART2->SR;
    const uint32_t control = USART2->CR1;
    BaseType_t higher_priority_woken = pdFALSE;

    if ((status & USART_SR_RXNE) != 0u && (control & USART_CR1_RXNEIE) != 0u) {
        const auto byte = static_cast<uint8_t>(USART2->DR);
        (void)rx_push_from_isr(byte);
        if (mtr::app().command != nullptr) {
            vTaskNotifyGiveFromISR(mtr::app().command, &higher_priority_woken);
        }
    } else if ((status & USART_SR_ORE) != 0u) {
        (void)USART2->DR;
        mtr::app().rx_dropped.fetch_add(1, std::memory_order_relaxed);
    }

    if ((status & USART_SR_TXE) != 0u && (control & USART_CR1_TXEIE) != 0u) {
        uint8_t byte = 0;
        if (tx_pop_from_isr(byte)) {
            USART2->DR = byte;
        } else {
            const uint32_t keep_rx = control & USART_CR1_RXNEIE;
            USART2->CR1 = (USART2->CR1 & ~USART_CR1_TXEIE) | keep_rx;
        }
    }

    portYIELD_FROM_ISR(higher_priority_woken);
}
