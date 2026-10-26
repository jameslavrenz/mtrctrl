#!/usr/bin/env python3
"""Hardware checks for the NUCLEO-F446RE image. Not run in CI.

The board must be on the ST-Link virtual COM port at 115200 8N1.
Opening the port resets a Nucleo. This script waits for the banner, then
drives the command interface. It writes a transcript and a telemetry CSV.
"""

import glob
import os
import select
import subprocess
import sys
import termios
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import telemetry_format as fmt


def find_port():
    requested = os.environ.get("MTR_PORT")
    if requested:
        return requested
    matches = sorted(glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/ttyACM*"))
    if not matches:
        raise SystemExit("no ST-Link virtual COM port found; set MTR_PORT")
    return matches[0]


class Port:
    def __init__(self, path):
        self.fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        attributes = termios.tcgetattr(self.fd)
        attributes[0] = 0
        attributes[1] = 0
        attributes[3] = 0
        crtscts = getattr(termios, "CRTSCTS", 0)
        attributes[2] &= ~(termios.CSIZE | termios.PARENB | termios.CSTOPB | crtscts)
        attributes[2] |= termios.CS8 | termios.CREAD | termios.CLOCAL
        attributes[4] = termios.B115200
        attributes[5] = termios.B115200
        attributes[6][termios.VMIN] = 0
        attributes[6][termios.VTIME] = 0
        termios.tcsetattr(self.fd, termios.TCSANOW, attributes)
        termios.tcflush(self.fd, termios.TCIOFLUSH)

    def write(self, text):
        view = text.encode("ascii")
        while view:
            count = os.write(self.fd, view)
            if count <= 0:
                raise OSError("short write")
            view = view[count:]
        termios.tcdrain(self.fd)

    def read_for(self, seconds):
        deadline = time.monotonic() + seconds
        chunks = []
        while time.monotonic() < deadline:
            ready, _, _ = select.select([self.fd], [], [], 0.05)
            if not ready:
                continue
            chunks.append(os.read(self.fd, 1024))
        return b"".join(chunks).decode("ascii", errors="replace")

    def close(self):
        os.close(self.fd)


def expect(port, command, seconds, transcript):
    if command is not None:
        port.write(command + "\r\n")
        transcript.append(">>> " + command)
    text = port.read_for(seconds)
    transcript.append(text)
    return text


def rows(text):
    # A read window can close in the middle of a 10 Hz line. Keep only rows
    # that have every column through cpu_percent.
    return [
        line
        for line in text.splitlines()
        if line.startswith("tel,") and line.count(",") >= len(fmt.COLUMNS) - 1
    ]


def field(row, index):
    parts = row.split(",")
    if len(parts) <= index:
        raise SystemExit("short telemetry row: " + row)
    return parts[index]


def status_value(text, name):
    prefix = name + " "
    for line in text.splitlines():
        if line.startswith(prefix):
            return line[len(prefix):]
    raise SystemExit("missing " + name + " in:\n" + text)


def reset_board():
    subprocess.run(
        [
            os.environ.get("OPENOCD", "openocd"),
            "-f",
            "interface/stlink.cfg",
            "-f",
            "firmware/openocd/nucleo_f446re.cfg",
            "-c",
            "init",
            "-c",
            "reset run",
            "-c",
            "exit",
        ],
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def main():
    path = find_port()
    print("port", path)
    port = Port(path)
    transcript = []
    try:
        reset_board()
        banner = expect(port, None, 2.0, transcript)
        if "type help" not in banner:
            raise SystemExit("banner missing:\n" + banner)
        help_text = expect(port, "help", 0.5, transcript)
        if "set speed" not in help_text or "ok" not in help_text:
            raise SystemExit("help failed:\n" + help_text)
        status = expect(port, "status", 0.4, transcript)
        if "state stopped" not in status:
            raise SystemExit("expected stopped after reset:\n" + status)

        expect(port, "start", 0.3, transcript)
        expect(port, "set speed 1000", 0.3, transcript)
        settling = expect(port, None, 1.2, transcript)
        running = expect(port, "status", 0.8, transcript)
        if "state running" not in running:
            raise SystemExit("controller did not stay running:\n" + settling + running)
        actual = None
        for line in running.splitlines():
            if line.startswith("actual_rpm "):
                actual = float(line.split()[1])
        if actual is None or actual < 800.0:
            raise SystemExit("speed did not approach 1000 RPM:\n" + running)

        expect(port, "telemetry on", 0.2, transcript)
        telem = expect(port, None, 1.2, transcript)
        expect(port, "telemetry off", 0.3, transcript)
        samples = rows(telem)
        if len(samples) < 5:
            raise SystemExit("too few telemetry rows:\n" + telem)
        misses = [int(field(row, fmt.column("deadline_misses"))) for row in samples]
        execution = [int(field(row, fmt.column("execution_us"))) for row in samples]
        wake = [int(field(row, fmt.column("wake_latency_us"))) for row in samples]
        jitter = [int(field(row, fmt.column("release_jitter_us"))) for row in samples]
        response = [int(field(row, fmt.column("response_us"))) for row in samples]
        if max(misses) != 0:
            raise SystemExit("deadline misses during the nominal run: " + str(misses))
        if max(execution) == 0:
            raise SystemExit("execution time stayed zero")
        print(
            "nominal samples",
            len(samples),
            "exec_us",
            min(execution),
            max(execution),
            "wake_us",
            min(wake),
            max(wake),
            "jitter_us",
            min(jitter),
            max(jitter),
            "response_us",
            min(response),
            max(response),
        )

        expect(port, "set load 0.02", 0.3, transcript)
        time.sleep(1.0)
        loaded = expect(port, "status", 0.4, transcript)
        if "state running" not in loaded:
            raise SystemExit("load disturbed the state machine:\n" + loaded)

        expect(port, "stop", 0.3, transcript)
        time.sleep(0.3)
        stopped = expect(port, "status", 0.4, transcript)
        if "state stopped" not in stopped:
            raise SystemExit("stop failed:\n" + stopped)

        expect(port, "start", 0.3, transcript)
        injected = expect(port, "fault inject feedback", 0.3, transcript)
        if "\nok" not in injected and not injected.strip().endswith("ok"):
            raise SystemExit(injected)
        time.sleep(0.2)
        faulted = expect(port, "status", 0.4, transcript)
        if "state faulted" not in faulted:
            raise SystemExit("feedback injection did not fault:\n" + faulted)
        feedback_flags = int(status_value(faulted, "fault_flags"), 0)
        if (feedback_flags & 2) == 0:
            raise SystemExit("feedback flag was not set:\n" + faulted)
        rejected = expect(port, "start", 0.3, transcript)
        if "error: faulted" not in rejected:
            raise SystemExit("start was accepted while faulted:\n" + rejected)
        expect(port, "reset faults", 0.3, transcript)
        cleared = expect(port, "status", 0.4, transcript)
        if "state stopped" not in cleared:
            raise SystemExit("reset faults did not return to stopped:\n" + cleared)

        expect(port, "start", 0.3, transcript)
        expect(port, "fault inject deadline", 0.3, transcript)
        time.sleep(0.6)
        late = expect(port, "status", 0.4, transcript)
        if "state faulted" not in late:
            raise SystemExit("deadline injection did not fault:\n" + late)
        # Bit 0 is the deadline latch, bit 5 is the overrun latch.
        late_flags = int(status_value(late, "fault_flags"), 0)
        late_misses = int(status_value(late, "deadline_misses"))
        if (late_flags & 0x01) == 0 or (late_flags & 0x20) == 0 or late_misses < 3:
            raise SystemExit("deadline injection did not latch both monitors:\n" + late)
        print("deadline inject flags", late_flags, "misses", late_misses)
        expect(port, "reset faults", 0.3, transcript)

        stats = expect(port, "stats", 0.5, transcript)
        if "stack_min_free_words" not in stats or "execution_max_us" not in stats:
            raise SystemExit("stats missing diagnostics:\n" + stats)
        print("stats excerpt:")
        for line in stats.splitlines():
            if line.startswith(("execution", "wake", "release", "response", "stack", "cpu", "deadline", "state")):
                print(" ", line)

        expect(port, "load on", 0.2, transcript)
        time.sleep(1.2)
        expect(port, "telemetry on", 0.2, transcript)
        loaded_cpu = expect(port, None, 1.2, transcript)
        expect(port, "telemetry off", 0.2, transcript)
        expect(port, "load off", 0.2, transcript)
        cpu_rows = rows(loaded_cpu)
        if cpu_rows:
            cpu = [int(field(row, fmt.column("cpu_percent"))) for row in cpu_rows]
            print("cpu_percent under load", min(cpu), max(cpu))

        out_dir = os.environ.get("MTR_LOG_DIR", "build/fw")
        os.makedirs(out_dir, exist_ok=True)
        transcript_path = os.path.join(out_dir, "hwtest.log")
        with open(transcript_path, "w", encoding="ascii", errors="replace") as handle:
            handle.write("".join(transcript))
        csv_path = os.path.join(out_dir, "hwtest-telemetry.csv")
        with open(csv_path, "w", encoding="ascii") as handle:
            handle.write(
                "kind,timestamp_ms,setpoint_rpm,actual_rpm,error_rpm,control_output,load_torque_nm,"
                "execution_us,wake_latency_us,release_jitter_us,response_us,deadline_misses,exec_overruns,"
                "queue_overflows,fault_flags,state,feedback_valid,cpu_percent\n"
            )
            for row in samples:
                handle.write(row + "\n")
        print("wrote", transcript_path)
        print("wrote", csv_path)
        print("hwtest passed")
    except SystemExit as error:
        print(error)
        print("--- transcript ---")
        sys.stdout.write("".join(transcript))
        if transcript and not transcript[-1].endswith("\n"):
            print()
        raise SystemExit(1) from None
    finally:
        port.close()


if __name__ == "__main__":
    try:
        main()
    except SystemExit:
        raise
    except Exception as error:
        print("hwtest failed:", error, file=sys.stderr)
        raise
