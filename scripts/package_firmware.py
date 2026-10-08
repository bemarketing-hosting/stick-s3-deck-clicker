"""Package the pinned Stick-S3 build as four NVS-preserving flash parts."""

import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys

import yaml

from generate_config import UniqueKeyLoader, render

ROOT = Path(__file__).resolve().parents[1]
FLASH_SIZE = 8 * 1024 * 1024
LAYOUT = (("bootloader.bin", 0, 0x8000),
          ("partitions.bin", 0x8000, 0x1000),
          ("boot_app0.bin", 0xE000, 0x2000),
          ("firmware.bin", 0x10000, 0x330000))


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def image_parser(core):
    tool = Path(core) / "packages/tool-esptoolpy"
    if not (tool / "esptool/bin_image.py").is_file():
        raise ValueError("PlatformIO esptool is missing; build firmware first")
    # PlatformIO vendors the uploader's dependencies in _contrib rather than
    # installing them into the project's Python virtual environment.
    sys.path[:0] = [str(tool), str(tool / "_contrib")]
    try:
        from esptool.bin_image import ESP32S3FirmwareImage
    except ImportError as exc:
        raise ValueError("Cannot load PlatformIO esptool; rebuild/install firmware tools") from exc
    return ESP32S3FirmwareImage


def validate_image(data, name, parser):
    # Pinned Arduino/PlatformIO emits DIO, 80 MHz, 8 MB headers even though
    # the board's memory type is qio_opi. Preserve these bytes when flashing.
    if len(data) < 24 or data[0] != 0xE9 or data[2:4] != b"\x02\x3f":
        raise ValueError(f"{name}: expected ESP image with DIO/80 MHz/8 MB header")
    if struct.unpack_from("<H", data, 12)[0] != 9:
        raise ValueError(f"{name}: expected ESP32-S3 chip ID")
    if not 1 <= data[1] <= 16 or data[23] != 1:
        raise ValueError(f"{name}: expected segments and appended SHA-256 digest")
    try:
        image = parser(io.BytesIO(data))
    except Exception as exc:
        raise ValueError(f"{name}: malformed or truncated ESP image: {exc}") from exc
    # esptool parses these fields without rejecting checksum/digest mismatches.
    if image.checksum != image.calculate_checksum():
        raise ValueError(f"{name}: ESP segment checksum mismatch")
    if image.stored_digest != image.calc_digest or len(image.stored_digest) != 32:
        raise ValueError(f"{name}: ESP image SHA-256 mismatch")
    if len(data) != image.data_length + 32:
        raise ValueError(f"{name}: unexpected bytes after ESP image digest")


def validate_partitions(data):
    expected = {"nvs": (1, 2, 0x9000, 0x5000),
                "otadata": (1, 0, 0xE000, 0x2000),
                "app0": (0, 0x10, 0x10000, 0x330000),
                "app1": (0, 0x11, 0x340000, 0x330000),
                "spiffs": (1, 0x82, 0x670000, 0x180000),
                "coredump": (1, 3, 0x7F0000, 0x10000)}
    actual = {}
    for position in range(0, len(data) - 31, 32):
        magic, kind, subtype, offset, size, label, flags = struct.unpack_from(
            "<HBBII16sI", data, position)
        if magic != 0x50AA:
            break
        name = label.rstrip(b"\0").decode("ascii")
        if name in actual or flags:
            raise ValueError("Unexpected partition flags or duplicate label")
        actual[name] = (kind, subtype, offset, size)
    if actual != expected:
        raise ValueError("Partition table differs from pinned default_8MB layout")


def package(project, output, version, commit=None, platformio_dir=None):
    project, output = Path(project), Path(output)
    if not re.fullmatch(r"v?\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?", version):
        raise ValueError("Version must be semantic version, e.g. v1.0.0")
    if commit is None:
        commit = subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=project, text=True).strip()
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise ValueError("Commit must be a full lowercase Git SHA")
    config_bytes = (project / "config.yaml").read_bytes()
    config = yaml.load(config_bytes.decode("utf-8"), Loader=UniqueKeyLoader)
    expected_header = render(config)
    if (project / "include/generated_config.h").read_text() != expected_header:
        raise ValueError("Generated configuration is stale; rebuild firmware")
    build = project / ".pio/build/sticks3"
    if (build / "firmware.bin").stat().st_mtime_ns < (project / "include/generated_config.h").stat().st_mtime_ns:
        raise ValueError("Firmware predates generated configuration; rebuild firmware")
    core = Path(platformio_dir or os.environ.get("PLATFORMIO_CORE_DIR", Path.home() / ".platformio"))
    boot_app = core / "packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
    blobs = {name: (boot_app if name == "boot_app0.bin" else build / name).read_bytes()
             for name, _, _ in LAYOUT}
    for name, offset, capacity in LAYOUT:
        data = blobs[name]
        if not data or len(data) > capacity:
            raise ValueError(f"{name}: empty or exceeds flash region")
        # Erase operations round to 4 KiB sectors. No part may touch NVS.
        end = (offset + len(data) + 0xFFF) & ~0xFFF
        if offset < 0xE000 and end > 0x9000:
            raise ValueError(f"{name}: overlaps Bluetooth bond storage")
    parser = image_parser(core)
    validate_image(blobs["bootloader.bin"], "bootloader.bin", parser)
    validate_image(blobs["firmware.bin"], "firmware.bin", parser)
    validate_partitions(blobs["partitions.bin"])
    if len(blobs["boot_app0.bin"]) != 0x2000:
        raise ValueError("Unexpected OTA initialization image size")
    manifest = {
        "schemaVersion": 1, "name": "Stick-S3 Deck Clicker", "version": version,
        "commit": commit, "configSha256": sha256(config_bytes),
        "chipFamily": "ESP32-S3", "flashSize": FLASH_SIZE, "config": config,
        "parts": [{"path": name, "offset": offset, "size": len(blobs[name]),
                   "sha256": sha256(blobs[name])} for name, offset, _ in LAYOUT],
    }
    # Validate everything before publishing any output. Manifest is written last.
    output.mkdir(parents=True, exist_ok=True)
    for name, data in blobs.items():
        (output / name).write_bytes(data)
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return manifest


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project", type=Path, default=ROOT)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--commit")
    args = parser.parse_args()
    try:
        result = package(args.project, args.output, args.version, args.commit)
    except (OSError, ValueError, yaml.YAMLError, subprocess.CalledProcessError) as exc:
        parser.exit(1, f"Packaging error: {exc}\n")
    print(f"Packaged {result['version']} ({result['commit'][:12]}) to {args.output}")
