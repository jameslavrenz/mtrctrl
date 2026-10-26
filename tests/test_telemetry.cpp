#include "mtr/telemetry_row.hpp"
#include "test_runner.hpp"

#include <fstream>
#include <string>

namespace {

std::string fixture_row() {
    const std::string path = std::string(MTR_SOURCE_DIR) + "/tests/fixtures/telemetry_row.txt";
    std::ifstream input(path);
    std::string line;
    std::getline(input, line);
    CHECK(input.good() || input.eof());
    CHECK(!line.empty());
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    return line;
}

std::string field_at(const std::string& line, unsigned index) {
    unsigned start = 0;
    unsigned field = 0;
    for (unsigned cursor = 0; cursor <= line.size(); ++cursor) {
        if (cursor == line.size() || line[cursor] == ',') {
            if (field == index) {
                return line.substr(start, cursor - start);
            }
            start = cursor + 1;
            ++field;
        }
    }
    return {};
}

unsigned column(const char* name) {
    for (unsigned index = 0; index < mtr::kTelemetryColumnCount; ++index) {
        if (std::string(mtr::kTelemetryColumns[index]) == name) {
            return index;
        }
    }
    CHECK(false);
    return 0;
}

void row_matches_fixture() {
    mtr::ControlSnapshot snapshot{};
    snapshot.timestamp_ms = 111;
    snapshot.setpoint_rpm = 222.f;
    snapshot.actual_rpm = 333.f;
    snapshot.error_rpm = 444.f;
    snapshot.control_output = 555.f;
    snapshot.load_torque_nm = 6.5f;
    snapshot.execution_us = 777;
    snapshot.wake_latency_us = 888;
    snapshot.release_jitter_us = 999;
    snapshot.response_us = 1010;
    snapshot.deadline_misses = 1111;
    snapshot.exec_overruns = 1212;
    snapshot.queue_overflows = 1313;
    snapshot.fault_flags = 1414;
    snapshot.state = 1515;
    snapshot.feedback_valid = 1616;

    char storage[256];
    mtr::TextBuffer buffer{storage, sizeof storage, 0};
    CHECK(mtr::format_telemetry_row(buffer, snapshot, 1717));
    CHECK(mtr::text_terminate(buffer));
    const std::string expected = fixture_row();
    CHECK(std::string(storage) == expected + "\r\n");
    CHECK(field_at(expected, column("execution_us")) == "777");
    CHECK(field_at(expected, column("deadline_misses")) == "1111");
    CHECK(field_at(expected, column("cpu_percent")) == "1717");
    CHECK(column("load_torque_nm") + 1 == column("execution_us"));
}

void column_comment_lists_every_name() {
    char storage[320];
    mtr::TextBuffer buffer{storage, sizeof storage, 0};
    CHECK(mtr::format_telemetry_column_comment(buffer));
    CHECK(mtr::text_terminate(buffer));
    const std::string text(storage);
    CHECK(text.rfind("# ", 0) == 0);
    unsigned position = 2;
    for (unsigned index = 0; index < mtr::kTelemetryColumnCount; ++index) {
        const std::string name(mtr::kTelemetryColumns[index]);
        CHECK(text.compare(position, name.size(), name) == 0);
        position += static_cast<unsigned>(name.size());
        if (index + 1u < mtr::kTelemetryColumnCount) {
            CHECK(text[position] == ',');
            ++position;
        }
    }
    CHECK(text.compare(position, 2, "\r\n") == 0);
}

const TestCase kTests[] = {
    {"row_matches_fixture", row_matches_fixture},
    {"column_comment_lists_every_name", column_comment_lists_every_name},
};

}  // namespace

int test_telemetry() {
    return run_suite("telemetry", kTests, static_cast<int>(sizeof kTests / sizeof kTests[0]));
}
