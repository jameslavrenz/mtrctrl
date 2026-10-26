"""Telemetry column order, taken from src/mtr/telemetry_row.hpp.

hwtest.py and plot_telemetry.py both import this module so a reordered
on-wire row cannot be parsed with a stale numeric index.
"""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src" / "mtr" / "telemetry_row.hpp"
FIXTURE = ROOT / "tests" / "fixtures" / "telemetry_row.txt"

# Distinctive values in tests/fixtures/telemetry_row.txt. The host test formats
# a snapshot with these same numbers and requires the fixture to match.
TOKENS = {
    "kind": "tel",
    "timestamp_ms": "111",
    "setpoint_rpm": "222.00",
    "actual_rpm": "333.00",
    "error_rpm": "444.00",
    "control_output": "555.000",
    "load_torque_nm": "6.5000",
    "execution_us": "777",
    "wake_latency_us": "888",
    "release_jitter_us": "999",
    "response_us": "1010",
    "deadline_misses": "1111",
    "exec_overruns": "1212",
    "queue_overflows": "1313",
    "fault_flags": "1414",
    "state": "1515",
    "feedback_valid": "1616",
    "cpu_percent": "1717",
}


def columns():
    text = HEADER.read_text(encoding="utf-8")
    block = re.search(r"kTelemetryColumns\[\] = \{([^}]+)\}", text)
    if block is None:
        raise RuntimeError("kTelemetryColumns not found in " + str(HEADER))
    names = re.findall(r'"([^"]+)"', block.group(1))
    if names != list(TOKENS):
        raise RuntimeError("telemetry column names do not match the fixture tokens")
    return names


COLUMNS = columns()


def column(name):
    return COLUMNS.index(name)


def fixture_row():
    return FIXTURE.read_text(encoding="ascii").strip()
