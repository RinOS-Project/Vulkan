#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Validate the repository-owned Vulkan ICD manifest template."""

import json
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) == 1:
        path = (Path(__file__).resolve().parents[1] / "icd.d" /
                "rin-vulkan.json.in")
        rendered = path.read_text(encoding="utf-8")
        rendered = rendered.replace(
            "@RIN_VULKAN_ICD_LIBRARY@", "librin-vulkan-icd.so")
        rendered = rendered.replace(
            "@RIN_VULKAN_ICD_LIBRARY_RELATIVE_DIR@", "../../../lib")
        manifest_path = None
        expected_library_path = "../../../lib/librin-vulkan-icd.so"
    elif len(sys.argv) in (2, 3):
        manifest_path = Path(sys.argv[1]).resolve()
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        icd = manifest.get("ICD")
        if not isinstance(icd, dict):
            raise SystemExit("invalid ICD manifest object")
        library_path = icd.get("library_path")
        if (not isinstance(library_path, str) or not library_path or
                Path(library_path).is_absolute()):
            raise SystemExit("ICD library path must be relative to the manifest")
        resolved_library = (manifest_path.parent / library_path).resolve()
        if not resolved_library.is_file():
            raise SystemExit("ICD library referenced by manifest does not exist")
        if len(sys.argv) == 3 and resolved_library != Path(sys.argv[2]).resolve():
            raise SystemExit("ICD manifest resolves to a different library")
        rendered = json.dumps(manifest)
        expected_library_path = library_path
    else:
        raise SystemExit("usage: check_manifest.py [manifest [expected-library]]")

    if manifest_path is None:
        if "@RIN_VULKAN_ICD_LIBRARY@" in rendered or (
                "@RIN_VULKAN_ICD_LIBRARY_RELATIVE_DIR@" in rendered):
            raise SystemExit("ICD manifest template contains an unconfigured token")
        manifest = json.loads(rendered)
    icd = manifest.get("ICD")
    if (manifest.get("file_format_version") != "1.0.0" or
            not isinstance(icd, dict) or icd.get("api_version") != "1.0.0" or
            icd.get("library_path") != expected_library_path):
        raise SystemExit("invalid RinVulkan ICD manifest")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
