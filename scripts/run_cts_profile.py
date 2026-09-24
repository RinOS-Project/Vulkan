#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run a bounded JSON command profile for RinVulkan host/CTS checks."""

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("catalog", type=Path)
    args = parser.parse_args()
    value = json.loads(args.catalog.read_text(encoding="utf-8"))
    cases = value.get("cases") if isinstance(value, dict) else None
    if not isinstance(cases, list) or not 1 <= len(cases) <= 32:
        raise SystemExit("invalid bounded RinVulkan CTS catalog")
    failures = 0
    for case in cases:
        command = case.get("command") if isinstance(case, dict) else None
        expected = case.get("expected") if isinstance(case, dict) else None
        timeout = case.get("timeout_seconds") if isinstance(case, dict) else None
        if (not isinstance(command, list) or not command or
                any(not isinstance(part, str) or not part for part in command) or
                expected not in {"PASS", "SKIP"} or
                not isinstance(timeout, int) or not 1 <= timeout <= 900):
            raise SystemExit("invalid bounded RinVulkan CTS case")
        if expected == "SKIP" and any(shutil.which(part) is None
                                      for part in command[:1]):
            continue
        try:
            result = subprocess.run(command, check=False, timeout=timeout)
        except (OSError, subprocess.TimeoutExpired):
            result = None
        if expected == "PASS" and (result is None or result.returncode != 0):
            failures += 1
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
