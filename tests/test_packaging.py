import hashlib
import json
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
        boot = self.core / "packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
        boot.parent.mkdir(parents=True)
        boot.write_bytes(b"\xff" * 0x2000)
        source = Path(__file__).resolve().parents[1] / "config.yaml"
        (self.root / "config.yaml").write_bytes(source.read_bytes())
        generate(self.root / "config.yaml", self.root / "include/generated_config.h")
        header = bytearray(24)
        header[0], header[2], header[3], header[12] = 0xE9, 2, 0x3F, 9
        for name in ("bootloader.bin", "firmware.bin"):
            (self.build / name).write_bytes(header + b"test payload")
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

    def test_missing_image_rejected(self):
        (self.build / "firmware.bin").unlink()
        with self.assertRaises(OSError):
            self.package()


if __name__ == "__main__":
    unittest.main()
