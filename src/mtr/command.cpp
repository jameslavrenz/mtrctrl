#include "mtr/command.hpp"

#include "mtr/units.hpp"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace mtr {
namespace {

constexpr unsigned kMaxLine = 80;

bool parse_float_token(const char* token, float& value) noexcept {
    if (token == nullptr || token[0] == '\0') {
        return false;
    }
    char* end = nullptr;
    const float parsed = std::strtof(token, &end);
    if (end == token || *end != '\0' || !std::isfinite(parsed)) {
        return false;
    }
    value = parsed;
    return true;
}

bool in_closed_range(float value, float minimum, float maximum) noexcept {
    return value >= minimum && value <= maximum;
}

}  // namespace

const char* help_text() noexcept {
    return
        "help\r\n"
        "status\r\n"
        "stats\r\n"
        "start\r\n"
        "stop\r\n"
        "set speed <rpm>          -1800..1800\r\n"
        "set kp <gain>\r\n"
        "set ki <gain>\r\n"
        "set kd <gain>\r\n"
        "set load <N.m>           -0.05..0.05\r\n"
        "reset pid\r\n"
        "reset faults\r\n"
        "telemetry on\r\n"
        "telemetry off\r\n"
        "load on\r\n"
        "load off\r\n"
        "load priority <1-4>\r\n"
        "fault inject deadline\r\n"
        "fault inject feedback\r\n";
}

ParsedCommand parse_command(const char* line) noexcept {
    ParsedCommand result{};
    if (line == nullptr) {
        result.ok = false;
        result.error = "missing command";
        return result;
    }

    while (*line != '\0' && std::isspace(static_cast<unsigned char>(*line))) {
        ++line;
    }
    if (*line == '\0') {
        return result;
    }

    char buffer[kMaxLine];
    unsigned length = 0;
    for (const char* cursor = line; *cursor != '\0'; ++cursor) {
        if (length + 1 >= kMaxLine) {
            result.ok = false;
            result.error = "command too long";
            return result;
        }
        buffer[length++] = static_cast<char>(std::tolower(static_cast<unsigned char>(*cursor)));
    }
    buffer[length] = '\0';

    char* tokens[4] = {};
    unsigned count = 0;
    char* cursor = buffer;
    while (*cursor != '\0') {
        while (*cursor != '\0' && std::isspace(static_cast<unsigned char>(*cursor))) {
            *cursor++ = '\0';
        }
        if (*cursor == '\0') {
            break;
        }
        if (count >= 4) {
            result.ok = false;
            result.error = "too many arguments";
            return result;
        }
        tokens[count++] = cursor;
        while (*cursor != '\0' && !std::isspace(static_cast<unsigned char>(*cursor))) {
            ++cursor;
        }
    }
    if (count == 0) {
        return result;
    }

    const char* a = tokens[0];
    const char* b = count > 1 ? tokens[1] : nullptr;
    const char* c = count > 2 ? tokens[2] : nullptr;

    auto fail = [&](const char* message) {
        result.ok = false;
        result.error = message;
        return result;
    };

    if (count == 1 && std::strcmp(a, "help") == 0) {
        result.kind = CommandKind::Help;
        return result;
    }
    if (count == 1 && std::strcmp(a, "status") == 0) {
        result.kind = CommandKind::Status;
        return result;
    }
    if (count == 1 && std::strcmp(a, "stats") == 0) {
        result.kind = CommandKind::Stats;
        return result;
    }
    if (count == 1 && std::strcmp(a, "start") == 0) {
        result.kind = CommandKind::Start;
        return result;
    }
    if (count == 1 && std::strcmp(a, "stop") == 0) {
        result.kind = CommandKind::Stop;
        return result;
    }
    if (count == 2 && std::strcmp(a, "reset") == 0 && std::strcmp(b, "pid") == 0) {
        result.kind = CommandKind::ResetPid;
        return result;
    }
    if (count == 2 && std::strcmp(a, "reset") == 0 && std::strcmp(b, "faults") == 0) {
        result.kind = CommandKind::ResetFaults;
        return result;
    }
    if (count == 2 && std::strcmp(a, "telemetry") == 0 && std::strcmp(b, "on") == 0) {
        result.kind = CommandKind::TelemetryOn;
        return result;
    }
    if (count == 2 && std::strcmp(a, "telemetry") == 0 && std::strcmp(b, "off") == 0) {
        result.kind = CommandKind::TelemetryOff;
        return result;
    }
    if (count == 2 && std::strcmp(a, "load") == 0 && std::strcmp(b, "on") == 0) {
        result.kind = CommandKind::LoadOn;
        return result;
    }
    if (count == 2 && std::strcmp(a, "load") == 0 && std::strcmp(b, "off") == 0) {
        result.kind = CommandKind::LoadOff;
        return result;
    }
    if (count == 3 && std::strcmp(a, "load") == 0 && std::strcmp(b, "priority") == 0) {
        float value = 0.f;
        if (!parse_float_token(c, value) || value != std::floor(value) || value < 1.f || value > 4.f) {
            return fail("priority out of range");
        }
        result.kind = CommandKind::LoadPriority;
        result.value = value;
        return result;
    }
    if (count == 3 && std::strcmp(a, "fault") == 0 && std::strcmp(b, "inject") == 0 &&
        std::strcmp(c, "deadline") == 0) {
        result.kind = CommandKind::InjectDeadline;
        return result;
    }
    if (count == 3 && std::strcmp(a, "fault") == 0 && std::strcmp(b, "inject") == 0 &&
        std::strcmp(c, "feedback") == 0) {
        result.kind = CommandKind::InjectFeedback;
        return result;
    }
    if (count == 3 && std::strcmp(a, "set") == 0) {
        float value = 0.f;
        if (!parse_float_token(c, value)) {
            return fail("invalid number");
        }
        if (std::strcmp(b, "speed") == 0) {
            if (!in_closed_range(value, -kMaxAbsSetpointRpm, kMaxAbsSetpointRpm)) {
                return fail("speed out of range");
            }
            result.kind = CommandKind::SetSpeed;
            result.value = value;
            return result;
        }
        if (std::strcmp(b, "kp") == 0) {
            if (!in_closed_range(value, 0.f, kMaxKp)) {
                return fail("kp out of range");
            }
            result.kind = CommandKind::SetKp;
            result.value = value;
            return result;
        }
        if (std::strcmp(b, "ki") == 0) {
            if (!in_closed_range(value, 0.f, kMaxKi)) {
                return fail("ki out of range");
            }
            result.kind = CommandKind::SetKi;
            result.value = value;
            return result;
        }
        if (std::strcmp(b, "kd") == 0) {
            if (!in_closed_range(value, 0.f, kMaxKd)) {
                return fail("kd out of range");
            }
            result.kind = CommandKind::SetKd;
            result.value = value;
            return result;
        }
        if (std::strcmp(b, "load") == 0) {
            if (!in_closed_range(value, -kMaxAbsLoadNm, kMaxAbsLoadNm)) {
                return fail("load out of range");
            }
            result.kind = CommandKind::SetLoad;
            result.value = value;
            return result;
        }
    }

    return fail("unknown command");
}

}  // namespace mtr
