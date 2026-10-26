#include "app.hpp"

#include "mtr/fault_policy.hpp"
#include "stm32f4xx_hal.h"

namespace mtr {

void supervisor_task(void* argument) {
    (void)argument;
    TickType_t last_wake = xTaskGetTickCount();
    uint32_t ticks = 0;
    uint32_t previous_samples = 0;
    uint32_t stalled = 0;
    uint32_t previous_idle = 0;
    uint32_t previous_total = 0;
    bool have_window = false;

    while (true) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(100));
        ++ticks;
        const ControlSnapshot snapshot = app().published.read();
        if (snapshot.state == static_cast<uint32_t>(RunState::Running)) {
            led_on();
        } else if (snapshot.state == static_cast<uint32_t>(RunState::Faulted)) {
            led_toggle();
        } else if ((ticks % 5u) == 0u) {
            led_toggle();
        }

        if (snapshot.state == static_cast<uint32_t>(RunState::Running) && snapshot.sample_count != 0u &&
            snapshot.sample_count == previous_samples) {
            ++stalled;
            if (stalled >= 3u) {
                app().safe_request.store(1, std::memory_order_release);
                control_clock_stop();
            }
        } else {
            stalled = 0;
        }
        previous_samples = snapshot.sample_count;

        if ((ticks % 10u) != 0u) {
            continue;
        }
        const uint32_t idle = ulTaskGetIdleRunTimeCounter();
        const uint32_t total = dwt_cycles();
        if (have_window) {
            const uint32_t idle_delta = idle - previous_idle;
            const uint32_t total_delta = total - previous_total;
            uint32_t percent = 0;
            if (total_delta != 0u && idle_delta < total_delta) {
                percent = static_cast<uint32_t>((static_cast<uint64_t>(total_delta - idle_delta) * 100ull) /
                                                total_delta);
            }
            app().cpu_percent.store(percent, std::memory_order_relaxed);
        }
        previous_idle = idle;
        previous_total = total;
        have_window = true;

        app().stack_free_words[0] = uxTaskGetStackHighWaterMark(app().control);
        app().stack_free_words[1] = uxTaskGetStackHighWaterMark(app().command);
        app().stack_free_words[2] = uxTaskGetStackHighWaterMark(app().supervisor);
        app().stack_free_words[3] = uxTaskGetStackHighWaterMark(app().telemetry);
        app().stack_free_words[4] = uxTaskGetStackHighWaterMark(app().load);
    }
}

void load_task(void* argument) {
    (void)argument;
    const uint32_t slice = SystemCoreClock / 2000u;
    while (true) {
        if (app().load_enabled.load(std::memory_order_relaxed) == 0u) {
            (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            continue;
        }
        const uint32_t start = dwt_cycles();
        while ((dwt_cycles() - start) < slice) {
        }
        // Time slicing is off. Yield so an equal-priority task can still run
        // `load off`. Idle stays preempted because this task does not block.
        taskYIELD();
    }
}

}  // namespace mtr
