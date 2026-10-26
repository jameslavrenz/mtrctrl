#include "app.hpp"

#include "mtr/fault_policy.hpp"
#include "mtr/text.hpp"
#include "mtr/units.hpp"
#include "stm32f4xx_hal.h"
#include "task.h"

#include <cstring>

namespace mtr {
namespace {

const char* state_name(uint32_t state) {
    switch (state) {
        case 1:
            return "running";
        case 2:
            return "faulted";
        default:
            return "stopped";
    }
}

void reply_error(const char* message) {
    uart_write("error: ");
    uart_write(message);
    uart_write("\r\n");
}

void append_snapshot(TextBuffer& buffer, const ControlSnapshot& snapshot) {
    (void)text_append(buffer, "state ");
    (void)text_append(buffer, state_name(snapshot.state));
    (void)text_append(buffer, "\r\nsetpoint_rpm ");
    (void)text_append_f(buffer, snapshot.setpoint_rpm, 2);
    (void)text_append(buffer, "\r\nactual_rpm ");
    (void)text_append_f(buffer, snapshot.actual_rpm, 2);
    (void)text_append(buffer, "\r\nerror_rpm ");
    (void)text_append_f(buffer, snapshot.error_rpm, 2);
    (void)text_append(buffer, "\r\ncontrol_output ");
    (void)text_append_f(buffer, snapshot.control_output, 3);
    (void)text_append(buffer, "\r\nload_torque_nm ");
    (void)text_append_f(buffer, snapshot.load_torque_nm, 4);
    (void)text_append(buffer, "\r\nfault_flags 0x");
    (void)text_append_hex(buffer, snapshot.fault_flags);
    (void)text_append(buffer, "\r\ndeadline_misses ");
    (void)text_append_u32(buffer, snapshot.deadline_misses);
    (void)text_append(buffer, "\r\nfeedback_valid ");
    (void)text_append_u32(buffer, snapshot.feedback_valid);
    (void)text_append(buffer, "\r\n");
}

void print_status() {
    char storage[320];
    TextBuffer buffer{storage, sizeof storage, 0};
    append_snapshot(buffer, app().published.read());
    (void)text_append(buffer, "ok\r\n");
    if (text_terminate(buffer)) {
        uart_write(storage);
    }
}

void print_stats() {
    const ControlSnapshot snapshot = app().published.read();
    char storage[640];
    TextBuffer buffer{storage, sizeof storage, 0};
    append_snapshot(buffer, snapshot);
    (void)text_append(buffer, "execution_us ");
    (void)text_append_u32(buffer, snapshot.execution_us);
    (void)text_append(buffer, "\r\nexecution_max_us ");
    (void)text_append_u32(buffer, snapshot.execution_max_us);
    (void)text_append(buffer, "\r\nwake_latency_us ");
    (void)text_append_u32(buffer, snapshot.wake_latency_us);
    (void)text_append(buffer, "\r\nwake_latency_max_us ");
    (void)text_append_u32(buffer, snapshot.wake_latency_max_us);
    (void)text_append(buffer, "\r\nrelease_jitter_us ");
    (void)text_append_i32(buffer, snapshot.release_jitter_us);
    (void)text_append(buffer, "\r\nrelease_jitter_abs_max_us ");
    (void)text_append_i32(buffer, snapshot.release_jitter_abs_max_us);
    (void)text_append(buffer, "\r\nresponse_us ");
    (void)text_append_u32(buffer, snapshot.response_us);
    (void)text_append(buffer, "\r\nresponse_max_us ");
    (void)text_append_u32(buffer, snapshot.response_max_us);
    (void)text_append(buffer, "\r\nexec_overruns ");
    (void)text_append_u32(buffer, snapshot.exec_overruns);
    (void)text_append(buffer, "\r\nqueue_overflows ");
    (void)text_append_u32(buffer, snapshot.queue_overflows);
    (void)text_append(buffer, "\r\ncpu_percent ");
    (void)text_append_u32(buffer, app().cpu_percent.load(std::memory_order_relaxed));
    (void)text_append(buffer, "\r\nrx_dropped ");
    (void)text_append_u32(buffer, app().rx_dropped.load(std::memory_order_relaxed));
    (void)text_append(buffer, "\r\nstack_min_free_words control ");
    (void)text_append_u32(buffer, app().stack_free_words[0]);
    (void)text_append(buffer, " command ");
    (void)text_append_u32(buffer, app().stack_free_words[1]);
    (void)text_append(buffer, " supervisor ");
    (void)text_append_u32(buffer, app().stack_free_words[2]);
    (void)text_append(buffer, " telemetry ");
    (void)text_append_u32(buffer, app().stack_free_words[3]);
    (void)text_append(buffer, " load ");
    (void)text_append_u32(buffer, app().stack_free_words[4]);
    (void)text_append(buffer, "\r\nok\r\n");
    if (text_terminate(buffer)) {
        uart_write(storage);
    }
}

void print_banner() {
    char storage[160];
    TextBuffer buffer{storage, sizeof storage, 0};
    (void)text_append(buffer, "mtrctrl NUCLEO-F446RE\r\nFreeRTOS ");
    (void)text_append(buffer, tskKERNEL_VERSION_NUMBER);
    (void)text_append(buffer, "\r\nSYSCLK ");
    (void)text_append_u32(buffer, SystemCoreClock);
    (void)text_append(buffer, " Hz\r\ncontrol ");
    (void)text_append_u32(buffer, kControlHz);
    (void)text_append(buffer, " Hz simulated plant\r\nphase 1: no physical motor\r\ntype help\r\n");
    (void)text_terminate(buffer);
    uart_write(storage);
}

bool queue_control(CommandKind kind, float value) {
    CommandMessage message{kind, value};
    if (xQueueSend(app().commands, &message, 0) != pdTRUE) {
        app().queue_overflows.fetch_add(1, std::memory_order_relaxed);
        reply_error("command queue full");
        return false;
    }
    uart_write("ok\r\n");
    return true;
}

void dispatch(const ParsedCommand& command) {
    if (!command.ok) {
        reply_error(command.error != nullptr ? command.error : "bad command");
        return;
    }
    switch (command.kind) {
        case CommandKind::None:
            return;
        case CommandKind::Help:
            uart_write(help_text());
            uart_write("ok\r\n");
            return;
        case CommandKind::Status:
            print_status();
            return;
        case CommandKind::Stats:
            print_stats();
            return;
        case CommandKind::TelemetryOn:
            app().telemetry_enabled.store(1, std::memory_order_release);
            uart_write("ok\r\n");
            return;
        case CommandKind::TelemetryOff:
            app().telemetry_enabled.store(0, std::memory_order_release);
            uart_write("ok\r\n");
            return;
        case CommandKind::LoadOn:
            app().load_enabled.store(1, std::memory_order_release);
            xTaskNotifyGive(app().load);
            uart_write("ok\r\n");
            return;
        case CommandKind::LoadOff:
            app().load_enabled.store(0, std::memory_order_release);
            xTaskNotifyGive(app().load);
            uart_write("ok\r\n");
            return;
        case CommandKind::LoadPriority:
            vTaskPrioritySet(app().load, static_cast<UBaseType_t>(command.value));
            uart_write("ok\r\n");
            return;
        case CommandKind::Start:
            if (app().published.read().state == static_cast<uint32_t>(RunState::Faulted)) {
                reply_error("faulted");
                return;
            }
            queue_control(command.kind, command.value);
            return;
        case CommandKind::InjectDeadline:
            if (app().published.read().state != static_cast<uint32_t>(RunState::Running)) {
                reply_error("not running");
                return;
            }
            queue_control(command.kind, command.value);
            return;
        default:
            queue_control(command.kind, command.value);
            return;
    }
}

}  // namespace

void command_task(void* argument) {
    (void)argument;
    // Enable the receiver before the banner. The transmit ISR updates CR1, and
    // setting RXNEIE in the middle of that update loses the bit.
    uart_enable_rx();
    print_banner();

    char line[80];
    unsigned length = 0;
    bool overflow = false;
    while (true) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        uint8_t byte = 0;
        while (uart_rx_pop(byte)) {
            if (byte == '\r') {
                continue;
            }
            if (byte != '\n') {
                if (length + 1 >= sizeof line) {
                    overflow = true;
                } else {
                    line[length++] = static_cast<char>(byte);
                }
                continue;
            }
            line[length] = '\0';
            if (overflow) {
                reply_error("command too long");
            } else {
                dispatch(parse_command(line));
            }
            length = 0;
            overflow = false;
        }
    }
}

}  // namespace mtr
