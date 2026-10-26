#!/usr/bin/env python3
"""Lock the host parsers to the firmware telemetry column order."""

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import telemetry_format as fmt


def main():
    row = fmt.fixture_row()
    parts = row.split(",")
    if len(parts) != len(fmt.COLUMNS):
        raise SystemExit("fixture has %d fields, header has %d" % (len(parts), len(fmt.COLUMNS)))
    for name, token in fmt.TOKENS.items():
        found = parts[fmt.column(name)]
        if found != token:
            raise SystemExit("%s is %s, fixture token is %s" % (name, found, token))

    hwtest = (fmt.ROOT / "tools" / "hwtest.py").read_text(encoding="utf-8")
    if re.search(r"field\(\s*row\s*,\s*\d+", hwtest):
        raise SystemExit("hwtest.py uses a numeric telemetry index")
    plot = (fmt.ROOT / "tools" / "plot_telemetry.py").read_text(encoding="utf-8")
    if "telemetry_format" not in plot:
        raise SystemExit("plot_telemetry.py does not use telemetry_format")
    print("telemetry columns", len(fmt.COLUMNS))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print("telemetry column check failed:", error, file=sys.stderr)
        raise SystemExit(1) from None
