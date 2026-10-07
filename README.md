# M5Stack Stick-S3 Deck Clicker

A small Bluetooth keyboard for advancing a slide deck. The Stick-S3's main
application button sends **Right Arrow**; its secondary application button sends
**Left Arrow**, or **Escape** when held. The screen shows Bluetooth status,
battery level, and the current button mappings.

This project targets the **M5Stack Stick-S3 (ESP32-S3)**, not the older
M5StickC/M5StickC Plus models. It uses PlatformIO, Arduino, M5Unified, and NimBLE.

## Build and flash

Install Python 3 and connect the Stick-S3 with a USB **data** cable. From this
repository:

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements.txt
.venv/bin/pio run -e sticks3
.venv/bin/pio device list
```

Find the Stick-S3 serial port in the device list, then replace the example port
below with its current path:

```sh
.venv/bin/pio run -e sticks3 -t upload --upload-port /dev/cu.usbmodem11301
.venv/bin/pio device monitor --port /dev/cu.usbmodem11301 --baud 115200
```

The serial-port name can change after unplugging, resetting, or entering download
mode. Exit the monitor with Ctrl+C before uploading again. Firmware dependencies
are pinned in `platformio.ini`. Install the Python requirements in the same
environment that runs PlatformIO: its build hook needs PyYAML.

Run the configuration tests and native gesture/keyboard transport tests with:

```sh
.venv/bin/python -m unittest discover -s tests -v
c++ -std=c++11 -Wall -Wextra -Werror -Iinclude tests/test_gesture.cpp -o /tmp/sticks3-gesture-tests
/tmp/sticks3-gesture-tests
c++ -std=c++17 -Wall -Wextra -Werror -Itests/ble -Iinclude tests/test_ble.cpp src/BleKeyboard.cpp -o /tmp/stick-s3-test-ble
/tmp/stick-s3-test-ble
```

## Pair and present

1. Power on the Stick-S3 and wait for its Bluetooth advertising status.
2. On your Mac, open **System Settings → Bluetooth**, select **Stick-S3 Clicker**,
   and connect. It presents itself as a keyboard.
3. Wait for the device to report that it is ready. Focus your presentation's
   browser window and enter slideshow mode.
4. Tap the main button to advance, tap the secondary button to go back, and hold
   the secondary button for 650 ms to send Escape.

The main and secondary buttons are application buttons A and B respectively;
the separate reset/power button is not a presentation control. Press that button
once to power on/reset, twice to power off, or hold it to enter download mode.
Key behavior also depends on which application has focus, so rehearse in the
actual slide deck.

Pairing information is retained across restarts. The device advertises again
after a disconnect so the Mac can reconnect. Button input while disconnected is
discarded, never saved for playback later.

To clear saved Bluetooth bonds, hold **both application buttons during startup
for three seconds** and follow the on-screen progress. Then forget the existing
device in macOS Bluetooth settings and pair again. This startup gesture does not
use the separate reset/power button.

## Customize with YAML

Edit `config.yaml`, rebuild, and flash. Configuration is compiled into firmware;
there is no runtime YAML upload or configuration server. The build validates the
file and generates `include/generated_config.h`, which must not be edited or
committed.

Default configuration:

```yaml
bluetooth:
  name: Stick-S3 Clicker
display:
  brightness: 64
gestures:
  debounce_ms: 25
  double_click_ms: 250
  long_press_ms: 650
buttons:
  main:
    single: RIGHT
    double: null
    long: null
  secondary:
    single: LEFT
    double: null
    long: ESC
```

Use YAML `null` to leave a gesture unassigned. Key names include arrows,
`ESC`, `ENTER`, `SPACE`, `TAB`, `BACKSPACE`, `HOME`, `END`, `PAGE_UP`, `PAGE_DOWN`,
letters, digits, and `F1`–`F12`. Mappings send one key press followed by a release;
macros, text strings, and modifier combinations are outside this MVP.

- A single action fires on release.
- A configured long press fires once at its threshold and suppresses the single
  action on release. An unassigned long press acts as one ordinary tap on release.
- With `double: null`, two quick taps send two single actions immediately on
  their releases.
- With a double-click mapping, the first single action waits for the configured
  double-click window. A recognized double-click sends only the double action.

For example, setting `main.double: SPACE` enables Space on a double-click, with
the corresponding delay before a single Right Arrow. Keep double-clicks
unassigned for the simplest and most responsive presentation controls.

The Bluetooth name must be 1–29 UTF-8 bytes. Brightness is 1–255. Gesture timing
must satisfy `debounce_ms < double_click_ms < long_press_ms`, with ranges of
1–100 ms, 50–1000 ms, and 100–10000 ms respectively. The build rejects invalid
settings before compiling the firmware.

Bluetooth stays awake and automatic sleep is disabled during use. Default
brightness is 64. Battery runtime is not guaranteed; verify it
with your device before relying on an unplugged presentation.

## Troubleshooting and recovery

- **No serial port:** try another data cable or USB port and rerun the device
  list. Confirm this is a Stick-S3 rather than an older Stick model.
- **Upload cannot connect:** close any serial monitor, reconnect the device, and
  confirm the new port. To enter download mode, connect USB and hold the side
  reset button until the internal green LED flashes, as described in the
  [official Stick-S3 instructions](https://docs.m5stack.com/en/core/StickS3).
  Reidentify the port and rerun the upload command. An application failure can usually be
  recovered by flashing from download mode.
- **Mac cannot reconnect:** first check the display and Bluetooth settings. If
  necessary, clear bonds with the startup gesture, forget the Mac's saved entry,
  and pair again.
- **Connected but slides do not move:** wait for readiness and check browser
  focus/slideshow mode. Test the arrows in another application to distinguish
  keyboard transport from the deck's shortcuts.
- **Configuration fails to build:** read the validation error, fix `config.yaml`,
  and rebuild. Do not edit the generated header to bypass validation.

## Presentation acceptance checklist

Implementation TODO and verification:

- [x] YAML generator, gesture engine, BLE keyboard, and device status display.
- [x] Pinned build setup, project instructions, and Git ignore rules.
- [x] Nine configuration tests plus native gesture and BLE transport suites pass.
- [x] Firmware compiled and uploaded with flash hashes verified.
- [x] Serial boot confirmed StickS3 detection, 8 MB flash/PSRAM, and advertising.

These physical checks require the device and the actual presentation. Treat each
as pending until you have personally confirmed it:

- [ ] Display works and reports connection/readiness correctly.
- [ ] Main tap advances exactly one slide; secondary tap goes back exactly once.
- [ ] Secondary hold exits without first navigating backward, and does not repeat.
- [ ] Rapid taps are reliable; no unwanted actions arrive after reconnecting.
- [ ] Mac reconnects after a device power cycle and after an idle period.
- [ ] The clicker works unplugged on battery for the required presentation time.
- [ ] The actual browser deck has focus and responds to Right, Left, and Escape.

Keep the laptop keyboard available as a fallback during the first presentation.
