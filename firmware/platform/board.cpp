#include "platform.hpp"

#include "stm32f4xx_hal.h"

extern "C" volatile const char* g_panic_reason;
extern "C" volatile const char* g_assert_file;
extern "C" volatile int g_assert_line;
extern "C" volatile const char* g_overflow_task;

namespace {

void configure_led_pin() {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    (void)RCC->AHB1ENR;
    GPIOA->MODER = (GPIOA->MODER & ~(3u << 10)) | (1u << 10);
    GPIOA->OTYPER &= ~(1u << 5);
    GPIOA->OSPEEDR &= ~(3u << 10);
    GPIOA->PUPDR &= ~(3u << 10);
}

// Polled, so a fault handler can report after interrupts are off. No effect
// when the console UART has not been switched on yet.
void panic_putc(char character) {
    if ((USART2->CR1 & USART_CR1_UE) == 0u) {
        return;
    }
    USART2->CR1 |= USART_CR1_TE;
    while ((USART2->SR & USART_SR_TXE) == 0u) {
    }
    USART2->DR = static_cast<uint8_t>(character);
}

void panic_puts(const char* text) {
    if (text == nullptr) {
        return;
    }
    while (*text != '\0') {
        panic_putc(*text++);
    }
}

void panic_puts(volatile const char* text) {
    if (text == nullptr) {
        return;
    }
    for (unsigned index = 0; index < 160u && text[index] != '\0'; ++index) {
        panic_putc(text[index]);
    }
}

void panic_hex(uint32_t value) {
    panic_puts("0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        const uint32_t nibble = (value >> shift) & 0xfu;
        panic_putc(static_cast<char>(nibble < 10u ? '0' + nibble : 'a' + nibble - 10u));
    }
}

}  // namespace

extern "C" void board_panic(const char* reason) {
    g_panic_reason = reason != nullptr ? reason : "panic";
    __disable_irq();
    panic_puts("\r\npanic: ");
    panic_puts(g_panic_reason);
    if (g_overflow_task != nullptr) {
        panic_puts(" task=");
        panic_puts(g_overflow_task);
    }
    if (g_assert_file != nullptr) {
        panic_puts(" at ");
        panic_puts(g_assert_file);
        panic_putc(':');
        panic_hex(static_cast<uint32_t>(g_assert_line));
    }
    panic_puts("\r\n");
    if ((CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) != 0u) {
        __BKPT(0);
    }
    configure_led_pin();
    while (true) {
        GPIOA->ODR ^= (1u << 5);
        for (uint32_t spin = 0; spin < 800000u; ++spin) {
            __NOP();
        }
    }
}

extern "C" void led_init() { configure_led_pin(); }

extern "C" void led_on() { GPIOA->BSRR = (1u << 5); }

extern "C" void led_off() { GPIOA->BSRR = (1u << (5 + 16)); }

extern "C" void led_toggle() { GPIOA->ODR ^= (1u << 5); }

uint32_t dwt_cycles() { return DWT->CYCCNT; }

uint32_t cycles_to_us(uint32_t cycles) {
    const uint32_t divisor = SystemCoreClock / 1000000u;
    return divisor == 0u ? 0u : cycles / divisor;
}

int32_t cycles_to_us_signed(int32_t cycles) {
    const int32_t divisor = static_cast<int32_t>(SystemCoreClock / 1000000u);
    return divisor == 0 ? 0 : cycles / divisor;
}

extern "C" void HardFault_HandlerC(uint32_t* frame) {
    const uint32_t pc = frame[6];
    const uint32_t lr = frame[5];
    __disable_irq();
    panic_puts("\r\nhardfault pc=");
    panic_hex(pc);
    panic_puts(" lr=");
    panic_hex(lr);
    panic_puts(" cfsr=");
    panic_hex(SCB->CFSR);
    panic_puts(" hfsr=");
    panic_hex(SCB->HFSR);
    panic_puts("\r\n");
    board_panic("hardfault");
}

extern "C" __attribute__((naked)) void HardFault_Handler(void) {
    __asm volatile("tst lr, #4\n"
                   "ite eq\n"
                   "mrseq r0, msp\n"
                   "mrsne r0, psp\n"
                   "b HardFault_HandlerC\n");
}
