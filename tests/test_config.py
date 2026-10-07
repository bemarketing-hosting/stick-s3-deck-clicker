import copy
import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("generate_config", ROOT / "scripts/generate_config.py")
config = importlib.util.module_from_spec(spec)
spec.loader.exec_module(config)


class ConfigTests(unittest.TestCase):
    def setUp(self):
        self.data = config.yaml.load((ROOT / "config.yaml").read_text(), Loader=config.UniqueKeyLoader)

    def test_defaults(self):
        result = config.render(self.data)
        self.assertIn("MAIN = {79, 0, 0}", result)
        self.assertIn("SECONDARY = {80, 0, 41}", result)

    def test_all_supported_keys_and_digits(self):
        for name, code in config.KEYS.items():
            self.assertEqual(config.keycode(name.lower(), "test"), code)
        self.assertEqual(config.keycode(1, "test"), 0x1E)
        self.assertEqual(config.keycode(0, "test"), 0x27)
        self.assertEqual(config.keycode(None, "test"), 0)

    def test_unicode_and_quotes_in_device_name(self):
        self.data["bluetooth"]["name"] = 'Slide "→" 🎤'
        result = config.render(self.data)
        self.assertIn('Slide \\"→\\" 🎤', result)
        self.assertNotIn("\\ud83c", result)

    def test_reject_unknown_keys(self):
        for value in ("RIGTH", "CTRL+C", True, [], 10):
            with self.subTest(value=value), self.assertRaises(ValueError):
                config.keycode(value, "buttons.main.single")

    def test_reject_missing_and_extra_fields(self):
        for section in ("bluetooth", "display", "gestures", "buttons"):
            data = copy.deepcopy(self.data)
            del data[section]
            with self.subTest(section=section), self.assertRaises(ValueError):
                config.render(data)
        self.data["buttons"]["main"]["triple"] = "RIGHT"
        with self.assertRaises(ValueError):
            config.render(self.data)

    def test_invalid_timing(self):
        for field, value in (("debounce_ms", 0), ("debounce_ms", True),
                             ("double_click_ms", 1500), ("long_press_ms", 250),
                             ("long_press_ms", "650")):
            data = copy.deepcopy(self.data)
            data["gestures"][field] = value
            with self.subTest(field=field, value=value), self.assertRaises(ValueError):
                config.render(data)

    def test_invalid_name_brightness(self):
        for name in ("", "a" * 30, "a\n", "😀" * 8, 123):
            self.data["bluetooth"]["name"] = name
            with self.subTest(name=name), self.assertRaises(ValueError):
                config.render(self.data)
        self.setUp()
        for brightness in (0, 256, True):
            self.data["display"]["brightness"] = brightness
            with self.assertRaises(ValueError):
                config.render(self.data)

    def test_reject_malformed_and_duplicate_yaml(self):
        with tempfile.TemporaryDirectory() as temp:
            source, destination = Path(temp) / "config.yaml", Path(temp) / "generated.h"
            for text in ("buttons: [", "bluetooth: null\nbluetooth: null", "null", "? [a,b]\n: c"):
                source.write_text(text)
                with self.subTest(text=text), self.assertRaises(ValueError):
                    config.generate(source, destination)
                self.assertFalse(destination.exists())

    def test_deterministic_output_and_no_write_when_unchanged(self):
        with tempfile.TemporaryDirectory() as temp:
            dest = Path(temp) / "include" / "generated.h"
            config.generate(ROOT / "config.yaml", dest)
            before = dest.stat().st_mtime_ns
            config.generate(ROOT / "config.yaml", dest)
            self.assertEqual(dest.stat().st_mtime_ns, before)


if __name__ == "__main__":
    unittest.main()
