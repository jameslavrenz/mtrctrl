#pragma once

#include <cstdint>

namespace mtr {

enum class CommandKind : uint8_t {
    None = 0,
    Help,
    Status,
    Stats,
    Start,
    Stop,
    ResetPid,
    ResetFaults,
    SetSpeed,
    SetKp,
    SetKi,
    SetKd,
    SetLoad,
    TelemetryOn,
    TelemetryOff,
    InjectDeadline,
    InjectFeedback,
    LoadOn,
    LoadOff,
    LoadPriority,
};

struct ParsedCommand {
    bool ok = true;
    CommandKind kind = CommandKind::None;
    float value = 0.f;
    const char* error = nullptr;
};

// `line` is a single command without the line ending. Empty input is ok and
// has kind None. `error` points at a static string when ok is false.
ParsedCommand parse_command(const char* line) noexcept;

const char* help_text() noexcept;

}  // namespace mtr
