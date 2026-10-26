#pragma once

#include <cstddef>
#include <cstdint>

#include "FreeRTOS.h"
#include "task.h"

extern "C" {
void board_panic(const char* reason);
void led_init();
void led_on();
void led_off();
void led_toggle();
}

bool clock_to_180mhz();
uint32_t apb1_timer_clock_hz();
uint32_t dwt_cycles();
uint32_t cycles_to_us(uint32_t cycles);
int32_t cycles_to_us_signed(int32_t cycles);

void uart_hw_init();
void uart_enable_rx();
void uart_write(const char* text);
bool uart_rx_pop(uint8_t& byte);

void control_clock_start(TaskHandle_t task);
void control_clock_stop();
uint32_t control_clock_last_isr();
int32_t control_clock_last_jitter();
bool control_clock_jitter_valid();
