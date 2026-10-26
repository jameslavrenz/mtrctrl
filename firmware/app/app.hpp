#pragma once

#include "mtr/command.hpp"
#include "mtr/snapshot.hpp"
#include "platform.hpp"

#include "queue.h"
#include "semphr.h"

#include <atomic>
#include <cstdint>

namespace mtr {

inline constexpr UBaseType_t kPriorityLoad = 1;
inline constexpr UBaseType_t kPriorityTelemetry = 2;
inline constexpr UBaseType_t kPrioritySupervisor = 3;
inline constexpr UBaseType_t kPriorityCommand = 4;
inline constexpr UBaseType_t kPriorityControl = 5;

inline constexpr uint32_t kControlStackWords = 512;
inline constexpr uint32_t kCommandStackWords = 768;
inline constexpr uint32_t kTelemetryStackWords = 512;
inline constexpr uint32_t kSupervisorStackWords = 384;
inline constexpr uint32_t kLoadStackWords = 256;
inline constexpr UBaseType_t kCommandQueueLength = 8;

struct CommandMessage {
    CommandKind kind;
    float value;
};

struct AppContext {
    TaskHandle_t control = nullptr;
    TaskHandle_t command = nullptr;
    TaskHandle_t telemetry = nullptr;
    TaskHandle_t supervisor = nullptr;
    TaskHandle_t load = nullptr;
    QueueHandle_t commands = nullptr;
    SemaphoreHandle_t tx_mutex = nullptr;
    SnapshotPublish published{};
    std::atomic<uint32_t> queue_overflows{0};
    std::atomic<uint32_t> cpu_percent{0};
    std::atomic<uint32_t> telemetry_enabled{0};
    std::atomic<uint32_t> load_enabled{0};
    std::atomic<uint32_t> safe_request{0};
    std::atomic<uint32_t> rx_dropped{0};
    uint32_t stack_free_words[5] = {};
};

AppContext& app() noexcept;
bool create_tasks();

void control_task(void* argument);
void command_task(void* argument);
void telemetry_task(void* argument);
void supervisor_task(void* argument);
void load_task(void* argument);

}  // namespace mtr
