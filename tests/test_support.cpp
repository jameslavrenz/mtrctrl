#include "mtr/command.hpp"
#include "mtr/snapshot.hpp"
#include "mtr/text.hpp"
#include "test_runner.hpp"

namespace {

void parses_known_commands() {
    const auto help = mtr::parse_command("Help");
    CHECK(help.ok);
    CHECK(help.kind == mtr::CommandKind::Help);
    const auto speed = mtr::parse_command("  set   speed  -250 ");
    CHECK(speed.ok);
    CHECK(speed.kind == mtr::CommandKind::SetSpeed);
    CHECK_NEAR(speed.value, -250.0, 1e-4);
    CHECK(mtr::parse_command("set kp 0.5").kind == mtr::CommandKind::SetKp);
    CHECK(mtr::parse_command("set ki 0").kind == mtr::CommandKind::SetKi);
    CHECK(mtr::parse_command("set kd 0.01").kind == mtr::CommandKind::SetKd);
    CHECK(mtr::parse_command("set load 0.02").kind == mtr::CommandKind::SetLoad);
    CHECK(mtr::parse_command("reset pid").kind == mtr::CommandKind::ResetPid);
    CHECK(mtr::parse_command("reset faults").kind == mtr::CommandKind::ResetFaults);
    CHECK(mtr::parse_command("telemetry on").kind == mtr::CommandKind::TelemetryOn);
    CHECK(mtr::parse_command("telemetry off").kind == mtr::CommandKind::TelemetryOff);
    CHECK(mtr::parse_command("fault inject deadline").kind == mtr::CommandKind::InjectDeadline);
    CHECK(mtr::parse_command("fault inject feedback").kind == mtr::CommandKind::InjectFeedback);
    CHECK(mtr::parse_command("load on").kind == mtr::CommandKind::LoadOn);
    CHECK(mtr::parse_command("load priority 4").kind == mtr::CommandKind::LoadPriority);
    CHECK(mtr::parse_command("start").kind == mtr::CommandKind::Start);
    CHECK(mtr::parse_command("stop").kind == mtr::CommandKind::Stop);
    CHECK(mtr::parse_command("stats").kind == mtr::CommandKind::Stats);
}

void rejects_bad_input() {
    CHECK(mtr::parse_command("") .ok);
    CHECK(mtr::parse_command("   ").kind == mtr::CommandKind::None);
    CHECK(!mtr::parse_command("nope").ok);
    CHECK(!mtr::parse_command("set speed 1800.1").ok);
    CHECK(!mtr::parse_command("set speed -1801").ok);
    CHECK(mtr::parse_command("set speed 1800").ok);
    CHECK(!mtr::parse_command("set kp -0.1").ok);
    CHECK(!mtr::parse_command("set load 1").ok);
    CHECK(!mtr::parse_command("set speed").ok);
    CHECK(!mtr::parse_command("load priority 5").ok);
    CHECK(!mtr::parse_command("load priority 1.5").ok);
    CHECK(!mtr::parse_command("fault inject fire").ok);
    CHECK(!mtr::parse_command("set speed 1 extra").ok);
    const auto huge = mtr::parse_command(
        "this command is far too long to be accepted by a bounded command line parser ok");
    CHECK(!huge.ok);
}

void formats_numbers() {
    char storage[32];
    mtr::TextBuffer buffer{storage, sizeof storage, 0};
    CHECK(mtr::text_append_i32(buffer, -42));
    CHECK(mtr::text_append(buffer, ','));
    CHECK(mtr::text_append_f(buffer, 1.25f, 2));
    CHECK(mtr::text_append(buffer, ','));
    CHECK(mtr::text_append_f(buffer, -0.5f, 1));
    CHECK(mtr::text_terminate(buffer));
    CHECK(std::string(storage) == "-42,1.25,-0.5");
}

void formats_non_finite_and_int_min() {
    char storage[32];
    mtr::TextBuffer buffer{storage, sizeof storage, 0};
    CHECK(mtr::text_append_f(buffer, 0.f / 0.f, 2));
    CHECK(mtr::text_terminate(buffer));
    CHECK(std::string(storage) == "nan");
    buffer.length = 0;
    CHECK(mtr::text_append_i32(buffer, static_cast<int32_t>(-2147483647 - 1)));
    CHECK(mtr::text_terminate(buffer));
    CHECK(std::string(storage) == "-2147483648");
    buffer.length = 0;
    CHECK(mtr::text_append_hex(buffer, 0));
    CHECK(mtr::text_append(buffer, ','));
    CHECK(mtr::text_append_hex(buffer, 0x21));
    CHECK(mtr::text_terminate(buffer));
    CHECK(std::string(storage) == "0,21");
}

void snapshot_round_trip() {
    mtr::SnapshotPublish publish;
    mtr::ControlSnapshot sample{};
    sample.timestamp_ms = 42;
    sample.setpoint_rpm = 1000.f;
    sample.actual_rpm = 995.5f;
    sample.deadline_misses = 7;
    sample.state = 1;
    publish.write(sample);
    sample.actual_rpm = 1.f;
    sample.sample_count = 9;
    publish.write(sample);
    const auto read = publish.read();
    CHECK(read.timestamp_ms == 42);
    CHECK(read.sample_count == 9);
    CHECK_NEAR(read.actual_rpm, 1.0, 1e-6);
    CHECK(read.deadline_misses == 7);
}

const TestCase kTests[] = {
    {"parses_known_commands", parses_known_commands},
    {"rejects_bad_input", rejects_bad_input},
    {"formats_numbers", formats_numbers},
    {"formats_non_finite_and_int_min", formats_non_finite_and_int_min},
    {"snapshot_round_trip", snapshot_round_trip},
};

}  // namespace

int test_support() {
    return run_suite("support", kTests, static_cast<int>(sizeof kTests / sizeof kTests[0]));
}
