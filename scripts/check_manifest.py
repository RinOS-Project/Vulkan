#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Validate the repository-owned Vulkan ICD manifest template."""

import json
from pathlib import Path


def main() -> int:
    path = Path(__file__).resolve().parents[1] / "icd.d" / "rin-vulkan.json.in"
    rendered = path.read_text(encoding="utf-8").replace(
        "@RIN_VULKAN_ICD_LIBRARY@", "librin-vulkan-icd.so")
    manifest = json.loads(rendered)
    icd = manifest.get("ICD")
    if (manifest.get("file_format_version") != "1.0.0" or
            not isinstance(icd, dict) or icd.get("api_version") != "1.0.0" or
            "$ORIGIN" not in icd.get("library_path", "")):
        raise SystemExit("invalid RinVulkan ICD manifest")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
