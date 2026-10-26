#include "app.hpp"

#include "mtr/telemetry_row.hpp"
#include "mtr/text.hpp"

namespace mtr {
namespace {

constexpr const char* kNotes =
    "# simulated: setpoint_rpm,actual_rpm,error_rpm,control_output,load_torque_nm\r\n"
    "# measured: timestamp_ms,execution_us,wake_latency_us,release_jitter_us,response_us,"
    "deadline_misses,exec_overruns,queue_overflows,cpu_percent\r\n";

}  // namespace

void telemetry_task(void* argument) {
    (void)argument;
    TickType_t last_wake = xTaskGetTickCount();
    bool announced = false;
    while (true) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(100));
        if (app().telemetry_enabled.load(std::memory_order_acquire) == 0u) {
            announced = false;
            continue;
        }
        if (!announced) {
            char header_storage[320];
            TextBuffer header{header_storage, sizeof header_storage, 0};
            if (format_telemetry_column_comment(header) && text_append(header, kNotes) && text_terminate(header)) {
                uart_write(header_storage);
                announced = true;
            }
        }
        char storage[256];
        TextBuffer buffer{storage, sizeof storage, 0};
        const ControlSnapshot snapshot = app().published.read();
        if (!format_telemetry_row(buffer, snapshot, app().cpu_percent.load(std::memory_order_relaxed)) ||
            !text_terminate(buffer)) {
            continue;
        }
        uart_write(storage);
    }
}

}  // namespace mtr
