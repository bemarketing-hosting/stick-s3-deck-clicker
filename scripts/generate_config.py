"""Validate config.yaml and emit the firmware's single configuration interface."""

import argparse
import json
from pathlib import Path

try:
    import yaml
except ImportError as exc:
    raise SystemExit(
        "Missing PyYAML. Run: python -m pip install -r requirements.txt "
        "using the Python environment that runs PlatformIO."
    ) from exc


class UniqueKeyLoader(yaml.SafeLoader):
    pass


def _mapping(loader, node, deep=False):
    result = {}
    for key_node, value_node in node.value:
        key = loader.construct_object(key_node, deep=deep)
        if not isinstance(key, str):
            raise ValueError("YAML mapping keys must be strings")
        if key in result:
            raise ValueError(f"Duplicate YAML field: {key}")
        result[key] = loader.construct_object(value_node, deep=deep)
    return result


UniqueKeyLoader.add_constructor(yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG, _mapping)

KEYS = {chr(ord("A") + n): 0x04 + n for n in range(26)}
KEYS.update({str(n): 0x1D + n for n in range(1, 10)})
KEYS["0"] = 0x27
KEYS.update({f"F{n}": 0x39 + n for n in range(1, 13)})
KEYS.update({
    "ENTER": 0x28, "ESC": 0x29, "ESCAPE": 0x29,
    "BACKSPACE": 0x2A, "TAB": 0x2B, "SPACE": 0x2C,
    "HOME": 0x4A, "PAGE_UP": 0x4B, "END": 0x4D, "PAGE_DOWN": 0x4E,
    "RIGHT": 0x4F, "LEFT": 0x50, "DOWN": 0x51, "UP": 0x52,
})


def fields(value, expected, location):
    if not isinstance(value, dict):
        raise ValueError(f"{location} must be a mapping")
    missing, extra = set(expected) - value.keys(), value.keys() - set(expected)
    if missing or extra:
        raise ValueError(f"{location}: missing fields {sorted(missing)}; unknown fields {sorted(extra)}")
    return value


def integer(value, low, high, location):
    if type(value) is not int or not low <= value <= high:
        raise ValueError(f"{location} must be an integer between {low} and {high}")
    return value


def keycode(value, location):
    if value is None:
        return 0
    # YAML naturally parses bare digit keys as integers. Accept these as keys.
    if type(value) is int and 0 <= value <= 9:
        value = str(value)
    if not isinstance(value, str) or value.upper() not in KEYS:
        raise ValueError(f"{location}: unsupported key {value!r}; use a named key or null")
    return KEYS[value.upper()]


def render(data):
    fields(data, ["bluetooth", "display", "gestures", "buttons"], "config")
    bluetooth = fields(data["bluetooth"], ["name"], "bluetooth")
    name = bluetooth["name"]
    if not isinstance(name, str) or not name.strip() or len(name.encode("utf-8")) > 29:
        raise ValueError("bluetooth.name must contain 1–29 UTF-8 bytes")
    if any(ord(c) < 32 or ord(c) == 127 for c in name):
        raise ValueError("bluetooth.name must not contain control characters")
    display = fields(data["display"], ["brightness"], "display")
    brightness = integer(display["brightness"], 1, 255, "display.brightness")
    timing = fields(data["gestures"], ["debounce_ms", "double_click_ms", "long_press_ms"], "gestures")
    debounce = integer(timing["debounce_ms"], 1, 100, "gestures.debounce_ms")
    double = integer(timing["double_click_ms"], 50, 1000, "gestures.double_click_ms")
    hold = integer(timing["long_press_ms"], 100, 10000, "gestures.long_press_ms")
    if not debounce < double < hold:
        raise ValueError("Gesture timing must satisfy debounce_ms < double_click_ms < long_press_ms")
    buttons = fields(data["buttons"], ["main", "secondary"], "buttons")
    mappings = {}
    for name_ in ("main", "secondary"):
        mapping = fields(buttons[name_], ["single", "double", "long"], f"buttons.{name_}")
        mappings[name_] = [keycode(mapping[event], f"buttons.{name_}.{event}") for event in ("single", "double", "long")]
    return "\n".join([
        "// Generated from config.yaml. Edit YAML, not this file.",
        "#pragma once", "#include <stdint.h>", "namespace ClickerConfig {",
        f"constexpr const char* DEVICE_NAME = {json.dumps(name, ensure_ascii=False)};",
        f"constexpr uint8_t BRIGHTNESS = {brightness};",
        f"constexpr uint32_t DEBOUNCE_MS = {debounce};",
        f"constexpr uint32_t DOUBLE_CLICK_MS = {double};",
        f"constexpr uint32_t LONG_PRESS_MS = {hold};",
        "struct ButtonMapping { uint8_t single, doubleClick, longPress; };",
        "constexpr ButtonMapping MAIN = {" + ", ".join(map(str, mappings["main"])) + "};",
        "constexpr ButtonMapping SECONDARY = {" + ", ".join(map(str, mappings["secondary"])) + "};",
        "} // namespace ClickerConfig", "",
    ])


def generate(source, destination):
    try:
        output = render(yaml.load(Path(source).read_text(encoding="utf-8"), Loader=UniqueKeyLoader))
    except (ValueError, yaml.YAMLError) as exc:
        raise ValueError(f"Invalid {source}: {exc}") from exc
    destination = Path(destination)
    if not destination.exists() or destination.read_text(encoding="utf-8") != output:
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(output, encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", default="config.yaml")
    parser.add_argument("--output", default="include/generated_config.h")
    args = parser.parse_args()
    try:
        generate(args.config, args.output)
    except (OSError, ValueError) as exc:
        parser.exit(1, f"Configuration error: {exc}\n")
