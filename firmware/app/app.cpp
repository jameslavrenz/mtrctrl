#include "app.hpp"

#include "semphr.h"

namespace mtr {
namespace {

static_assert(kPriorityControl > kPriorityCommand, "control must preempt the command task");
static_assert(kPriorityCommand > kPrioritySupervisor, "command handling preempts supervision");
static_assert(kPrioritySupervisor > kPriorityTelemetry, "supervision preempts telemetry output");
static_assert(kPriorityTelemetry > kPriorityLoad, "telemetry preempts the CPU-load task");
static_assert(kPriorityLoad > tskIDLE_PRIORITY, "load sits above the idle task");

AppContext g_app{};

StackType_t g_control_stack[kControlStackWords];
StackType_t g_command_stack[kCommandStackWords];
StackType_t g_telemetry_stack[kTelemetryStackWords];
StackType_t g_supervisor_stack[kSupervisorStackWords];
StackType_t g_load_stack[kLoadStackWords];
StaticTask_t g_control_tcb;
StaticTask_t g_command_tcb;
StaticTask_t g_telemetry_tcb;
StaticTask_t g_supervisor_tcb;
StaticTask_t g_load_tcb;

uint8_t g_command_queue_storage[kCommandQueueLength * sizeof(CommandMessage)];
StaticQueue_t g_command_queue;
StaticSemaphore_t g_tx_mutex;

}  // namespace

AppContext& app() noexcept { return g_app; }

bool create_tasks() {
    g_app.commands = xQueueCreateStatic(kCommandQueueLength, sizeof(CommandMessage), g_command_queue_storage,
                                        &g_command_queue);
    g_app.tx_mutex = xSemaphoreCreateMutexStatic(&g_tx_mutex);
    g_app.control = xTaskCreateStatic(control_task, "control", kControlStackWords, nullptr, kPriorityControl,
                                      g_control_stack, &g_control_tcb);
    g_app.command = xTaskCreateStatic(command_task, "command", kCommandStackWords, nullptr, kPriorityCommand,
                                      g_command_stack, &g_command_tcb);
    g_app.supervisor = xTaskCreateStatic(supervisor_task, "supervisor", kSupervisorStackWords, nullptr,
                                         kPrioritySupervisor, g_supervisor_stack, &g_supervisor_tcb);
    g_app.telemetry = xTaskCreateStatic(telemetry_task, "telemetry", kTelemetryStackWords, nullptr,
                                        kPriorityTelemetry, g_telemetry_stack, &g_telemetry_tcb);
    g_app.load = xTaskCreateStatic(load_task, "load", kLoadStackWords, nullptr, kPriorityLoad, g_load_stack,
                                   &g_load_tcb);
    return g_app.commands != nullptr && g_app.tx_mutex != nullptr && g_app.control != nullptr &&
           g_app.command != nullptr && g_app.supervisor != nullptr && g_app.telemetry != nullptr &&
           g_app.load != nullptr;
}

}  // namespace mtr
