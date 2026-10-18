import hashlib
import json
import os
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from generate_config import generate
from package_firmware import package


class PackagingTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.build = self.root / ".pio/build/sticks3"
        self.build.mkdir(parents=True)
        self.output = self.root / "output"
        self.core = self.root / "platformio"
        real_core = Path(os.environ.get("PLATFORMIO_CORE_DIR", Path.home() / ".platformio"))
        boot = self.core / "packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
        boot.parent.mkdir(parents=True)
        boot.write_bytes(b"\xff" * 0x2000)
        (self.core / "packages/tool-esptoolpy").symlink_to(
            real_core / "packages/tool-esptoolpy", target_is_directory=True)
        source = Path(__file__).resolve().parents[1] / "config.yaml"
        (self.root / "config.yaml").write_bytes(source.read_bytes())
        generate(self.root / "config.yaml", self.root / "include/generated_config.h")
        header = bytearray(24)
        header[0], header[1], header[2], header[3], header[12], header[23] = 0xE9, 1, 2, 0x3F, 9, 1
        # One DRAM segment, ESP XOR checksum at the next 16-byte boundary's
        # final byte, then the SHA-256 of the complete image through checksum.
        payload = b"test payload"
        checksum = 0xEF
        for byte in payload:
            checksum ^= byte
        image = header + struct.pack("<II", 0x3FC88000, len(payload)) + payload
        image += b"\0" * (15 - len(image) % 16) + bytes([checksum])
        image += hashlib.sha256(image).digest()
        for name in ("bootloader.bin", "firmware.bin"):
            (self.build / name).write_bytes(image)
        partitions = [("nvs", 1, 2, 0x9000, 0x5000),
                      ("otadata", 1, 0, 0xE000, 0x2000),
                      ("app0", 0, 0x10, 0x10000, 0x330000),
                      ("app1", 0, 0x11, 0x340000, 0x330000),
                      ("spiffs", 1, 0x82, 0x670000, 0x180000),
                      ("coredump", 1, 3, 0x7F0000, 0x10000)]
        (self.build / "partitions.bin").write_bytes(b"".join(
            struct.pack("<HBBII16sI", 0x50AA, kind, subtype, offset, size,
                        name.encode(), 0)
            for name, kind, subtype, offset, size in partitions))

    def package(self, **kwargs):
        return package(self.root, self.output, "v1.0.0", "a" * 40,
                       platformio_dir=self.core, **kwargs)

    def test_manifest_and_parts_are_deterministic_and_preserve_nvs(self):
        manifest = self.package()
        original = (self.output / "manifest.json").read_bytes()
        self.assertEqual(manifest["chipFamily"], "ESP32-S3")
        self.assertEqual(manifest["flashSize"], 8388608)
        self.assertEqual([p["offset"] for p in manifest["parts"]],
                         [0, 0x8000, 0xE000, 0x10000])
        for part in manifest["parts"]:
            data = (self.output / part["path"]).read_bytes()
            self.assertEqual(part["size"], len(data))
            self.assertEqual(part["sha256"], hashlib.sha256(data).hexdigest())
            self.assertTrue(part["offset"] >= 0xE000 or
                            part["offset"] + part["size"] <= 0x9000)
        self.package()
        self.assertEqual(original, (self.output / "manifest.json").read_bytes())
        self.assertEqual(json.loads(original), manifest)

    def test_wrong_chip_or_flash_parameters_fail_before_output(self):
        path = self.build / "firmware.bin"
        original = path.read_bytes()
        for index, value in ((12, 0), (2, 0), (3, 0x2F), (0, 0)):
            with self.subTest(index=index):
                data = bytearray(original)
                data[index] = value
                path.write_bytes(data)
                with self.assertRaises(ValueError):
                    self.package()
                self.assertFalse(self.output.exists())

    def test_changed_partition_layout_rejected(self):
        path = self.build / "partitions.bin"
        data = bytearray(path.read_bytes())
        struct.pack_into("<I", data, 4, 0xA000)
        path.write_bytes(data)
        with self.assertRaisesRegex(ValueError, "Partition table"):
            self.package()

    def test_oversized_bootloader_cannot_erase_nvs(self):
        path = self.build / "bootloader.bin"
        path.write_bytes(path.read_bytes() + b"\xff" * 0x9000)
        with self.assertRaisesRegex(ValueError, "exceeds flash region"):
            self.package()

    def test_stale_configuration_rejected(self):
        (self.root / "include/generated_config.h").write_text("stale")
        with self.assertRaisesRegex(ValueError, "stale"):
            self.package()

    def test_regenerated_configuration_without_rebuild_rejected(self):
        source = self.root / "config.yaml"
        source.write_text(source.read_text().replace("brightness: 64", "brightness: 65"))
        header = self.root / "include/generated_config.h"
        generate(source, header)
        os.utime(self.build / "firmware.bin", ns=(1_000_000_000, 1_000_000_000))
        os.utime(header, ns=(2_000_000_000, 2_000_000_000))
        with self.assertRaisesRegex(ValueError, "predates generated configuration"):
            self.package()

    def test_yaml_comment_change_does_not_require_recompile(self):
        source = self.root / "config.yaml"
        source.write_text(source.read_text() + "\n# Documentation-only change\n")
        self.package()

    def test_truncated_images_rejected(self):
        path = self.build / "firmware.bin"
        original = path.read_bytes()
        for length in (24, 35, 47, len(original) - 1):
            with self.subTest(length=length):
                path.write_bytes(original[:length])
                with self.assertRaisesRegex(ValueError, "malformed|SHA-256"):
                    self.package()
                self.assertFalse(self.output.exists())

    def test_segment_corruption_rejected(self):
        path = self.build / "firmware.bin"
        image = bytearray(path.read_bytes())
        image[32] ^= 1
        path.write_bytes(image)
        with self.assertRaisesRegex(ValueError, "segment checksum"):
            self.package()

    def test_digest_corruption_and_unexpected_trailing_bytes_rejected(self):
        path = self.build / "firmware.bin"
        original = path.read_bytes()
        image = bytearray(original)
        image[-1] ^= 1
        path.write_bytes(image)
        with self.assertRaisesRegex(ValueError, "SHA-256"):
            self.package()
        path.write_bytes(original + b"unexpected")
        with self.assertRaisesRegex(ValueError, "unexpected bytes"):
            self.package()

    def test_missing_esptool_explains_build_requirement(self):
        (self.core / "packages/tool-esptoolpy").unlink()
        with self.assertRaisesRegex(ValueError, "build firmware first"):
            self.package()

    def test_missing_image_rejected(self):
        (self.build / "firmware.bin").unlink()
        with self.assertRaises(OSError):
            self.package()


if __name__ == "__main__":
    unittest.main()
