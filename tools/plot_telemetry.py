#!/usr/bin/env python3
"""Plot a telemetry CSV captured from the board.

The STM32 does not plot. This script runs on the development computer.
Input is the CSV written by tools/hwtest.py, or any log that uses the same
columns. Rows that do not start with "tel," are ignored, so a raw serial
transcript can be passed as well.
"""

import argparse
import csv
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import telemetry_format as fmt

COLUMNS = fmt.COLUMNS


def load(path):
    rows = []
    with open(path, encoding="ascii", errors="replace") as handle:
        for raw in handle:
            line = raw.strip()
            if line.startswith("#"):
                continue
            if line.startswith("tel,"):
                line = line
            elif "," not in line:
                continue
            parts = next(csv.reader([line]))
            if len(parts) < len(COLUMNS) or parts[0] != "tel":
                continue
            rows.append({name: parts[index] for index, name in enumerate(COLUMNS)})
    return rows


def column(rows, name, cast):
    return [cast(row[name]) for row in rows]


def main():
    parser = argparse.ArgumentParser(description="Plot mtrctrl telemetry")
    parser.add_argument("csv", help="telemetry CSV or a serial transcript containing tel, rows")
    parser.add_argument("-o", "--output", default="telemetry.png", help="output image path")
    args = parser.parse_args()

    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib is not installed. pip install -r tools/requirements.txt", file=sys.stderr)
        return 1

    rows = load(args.csv)
    if not rows:
        print("no tel, rows in " + args.csv, file=sys.stderr)
        return 1

    t = [value / 1000.0 for value in column(rows, "timestamp_ms", float)]
    t0 = t[0]
    t = [value - t0 for value in t]

    figure, axes = plt.subplots(3, 1, figsize=(9, 8), sharex=True)
    axes[0].plot(t, column(rows, "setpoint_rpm", float), label="setpoint")
    axes[0].plot(t, column(rows, "actual_rpm", float), label="actual (simulated)")
    axes[0].set_ylabel("RPM")
    axes[0].legend()
    axes[0].grid(True, alpha=0.3)

    axes[1].plot(t, column(rows, "control_output", float), label="normalized torque")
    axes[1].set_ylabel("command")
    axes[1].legend()
    axes[1].grid(True, alpha=0.3)

    axes[2].plot(t, column(rows, "execution_us", int), label="execution")
    axes[2].plot(t, column(rows, "wake_latency_us", int), label="wake latency")
    axes[2].plot(t, column(rows, "release_jitter_us", int), label="release jitter")
    axes[2].plot(t, column(rows, "response_us", int), label="response")
    axes[2].set_ylabel("microseconds")
    axes[2].set_xlabel("seconds from first sample")
    axes[2].legend()
    axes[2].grid(True, alpha=0.3)

    figure.tight_layout()
    figure.savefig(args.output, dpi=120)
    print("wrote", args.output)
    return 0


if __name__ == "__main__":
    sys.exit(main())
