# M5Stack Stick-S3 Deck Clicker

A small Bluetooth keyboard for advancing a slide deck. The Stick-S3's main
application button sends **Right Arrow**; its secondary application button sends
**Left Arrow**, or **Escape** when held. The screen shows Bluetooth status,
battery level, and the current button mappings.

This project targets the **M5Stack Stick-S3 (ESP32-S3)**, not the older
M5StickC/M5StickC Plus models. It uses PlatformIO, Arduino, M5Unified, and NimBLE.

## Install from your browser

The Astro app in `web/` installs the latest official firmware over USB, with no
PlatformIO installation needed by visitors. The public GitHub Pages release is
published separately; until then, use the local preview instructions below.

1. Open the flasher in desktop Chrome or Edge and connect a **Stick-S3** with a
   USB **data** cable. Close serial monitors and other apps using its port.
2. Choose **Connect device** and select its current USB serial port. The flasher
   checks for an ESP32-S3 with 8 MB flash. These checks identify compatible
   silicon, not the board model; confirm that your physical device is a Stick-S3.
3. Review the firmware and select **Install firmware**. Keep the device connected
   until writing and verification finish.
4. Follow the pairing instructions on the page and test the controls in your
   presentation app.

Normal reflashing preserves the pairing-storage region for this firmware's
partition layout. The optional **Clean installation** erases all device storage,
including Bluetooth bonds; forget the old computer-side Bluetooth entry and pair
again afterward. Installing the official firmware replaces any locally compiled
custom mappings with the release's settings.

The flasher needs HTTPS or localhost and browser Web Serial support. Desktop
Chrome and Edge are the initial targets; mobile flashing is not supported in this
version. If automatic reset fails, follow the page's manual restart instructions.
The command-line build/upload process below remains available for recovery and
custom YAML configurations.

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

Run a firmware build first: the packaging tests also use the pinned esptool
image parser that PlatformIO installs with the build tools.

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
- [x] The owner successfully used the clicker for a real presentation.

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

## Repository layout and web development

Firmware stays at the repository root; `web/` is an independently built Astro
app. There is no workspace manager or runtime server. Reusable Astro components,
central product metadata, CSS theme variables, and a separate TypeScript USB
controller keep the website adaptable to another project.

Install a current Node.js LTS release compatible with the version declared in
`web/package.json`, then prepare a local firmware bundle from the root:

```sh
.venv/bin/python -m pip install -r requirements.txt
.venv/bin/pio run -e sticks3
.venv/bin/python scripts/package_firmware.py --version 0.0.0-dev --output web/public/firmware
npm --prefix web ci
npm --prefix web run dev
```

Open the localhost URL printed by Astro, including `/stick-s3-deck-clicker/`.
Local development firmware is labeled as a development version. Rebuild and
repackage after editing `config.yaml` so the page and installed firmware agree.
Generated firmware bundles, headers, dependency directories, and site output are
ignored by Git.

```sh
npm --prefix web run check
npm --prefix web test
npm --prefix web run build
npm --prefix web run preview
```

The production build requires a packaged manifest and binaries. Release metadata
and displayed button mappings come from that bundle, rather than duplicated
firmware settings in the website. The browser verifies the binary checksums
before writing and verifies flash contents afterward. All installation assets
are hosted alongside the site.

## Publish a stable release

The manual **Publish stable firmware and Pages** workflow accepts an existing
stable tag such as `v1.0.0`. It checks out that tag, runs the firmware and website
tests, builds the firmware, generates the bundle, and builds Astro using that
same bundle. GitHub Releases receives the manifest and four binaries; Pages
receives the site with identical firmware files. Visitors install the currently
published stable version; there is no development-build or version selector.

Before the first publication, set the repository's **Settings → Pages → Build
and deployment → Source** to **GitHub Actions**. The expected site URL is
`https://bemarketing-hosting.github.io/stick-s3-deck-clicker/`.

After reviewing and authorizing the changes for publication:

1. Push the reviewed commits and a new stable `vX.Y.Z` tag containing the web
   flasher and workflows. Ordinary pushes run validation, not deployment.
2. In **Actions**, select **Publish stable firmware and Pages → Run workflow**
   and enter that existing tag as `version`.
3. Confirm the release and Pages jobs succeed, then check the live site's version,
   firmware downloads, and installation flow.

Published versions cannot be overwritten. Publish a new version for corrections.
If release creation succeeds but Pages deployment fails, rerun only the failed
deployment job; rerunning the full release job intentionally rejects the existing
release. No public release or Git push is performed by local build commands.

## Web-flasher physical acceptance

Software tests do not establish that browser flashing works on a physical device.
The owner will perform these checks; do not mark them complete based on a build:

- [ ] Install from a desktop browser, then confirm boot, display, pairing, and
      all three default actions in an actual presentation.
- [ ] Reflash normally and confirm existing Bluetooth pairing still works.
- [ ] Select clean installation, confirm erase and successful boot, forget the
      computer's old Bluetooth entry, and pair again.
- [ ] Confirm the recovery instructions are usable if download mode or manual
      restart is needed.

See [TODO.md](TODO.md) for implementation and verification progress.

Software verification for the web flasher: 21 Python configuration/packaging
tests and 11 TypeScript flasher tests pass, as do the native gesture/BLE suites,
the Stick-S3 firmware build, Astro checks, and the production site build.
Browser checks covered desktop/mobile layouts, keyboard navigation, missing or
changed releases, unsupported browsers, and cancelled port selection. These
checks used no hardware writes. GitHub-hosted workflow execution and physical
web flashing remain pending their first run.
