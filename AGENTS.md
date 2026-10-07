# Stick-S3 Deck Clicker

## Working agreements

- Target the M5Stack Stick-S3 (ESP32-S3), using the pinned PlatformIO/Arduino,
  M5Unified, M5GFX, and NimBLE dependencies in `platformio.ini`.
- Keep this a small, dependable presentation clicker. Prefer clear modules over
  extra features. Do not add network services or runtime configuration systems
  without a request.
- `config.yaml` is the source of truth for key mappings, gesture timing,
  Bluetooth name, and display brightness. Validate it at build time; never
  hardcode configurable mappings in firmware or commit generated headers.
- Keep hardware/display, gesture recognition, BLE keyboard transport, and
  configuration generation separate. Gesture logic should be testable without
  physical hardware.
- Preserve one-shot long presses, release suppression after a long press,
  explicit HID key release, and discarding input during disconnection.
- Keep Bluetooth awake for presenting. Do not introduce automatic sleep without
  considering missed input and reconnect delay.
- Use a TODO list and focused local commits with descriptive titles and bodies.
  Keep parallel workstreams on distinct files; coordinate shared interfaces.
- Preserve user changes. Do not push commits unless requested.

## Build and validation

Create a Python virtual environment and install `requirements.txt` into it.
Use `.venv/bin/pio run -e sticks3` for a firmware build and
`.venv/bin/pio device list` to identify serial devices. The PlatformIO Python
environment must contain PyYAML for the pre-build generator.

Run configuration and gesture tests when changing those behaviors. Verify a
firmware build after integration. Do not claim a successful compile proves
Bluetooth pairing, button identity, display rendering, or battery runtime.

Before flashing, reidentify the intended USB device and verify its chip/flash
identity. Flash only within the user's authorization. Port names are examples,
not stable device identifiers. Close serial monitors before uploading. Report
physical checks as pending unless observed or confirmed by the user.

Keep README setup, YAML examples, gesture semantics, and recovery instructions
aligned with the implementation. Document useful verification results and known
limitations without making unmeasured battery-life claims.

<!-- CODEGRAPH_START -->
## CodeGraph

In repositories indexed by CodeGraph (a `.codegraph/` directory exists at the repo root), reach for it BEFORE grep/find or reading files when you need to understand or locate code:

- **MCP tool** (when available): `codegraph_explore` answers most code questions in one call — the relevant symbols' verbatim source plus the call paths between them, including dynamic-dispatch hops grep can't follow. Name a file or symbol in the query to read its current line-numbered source. If it's listed but deferred, load it by name via tool search.
- **Shell** (always works): `codegraph explore "<symbol names or question>"` prints the same output.

If there is no `.codegraph/` directory, skip CodeGraph entirely — indexing is the user's decision.
<!-- CODEGRAPH_END -->
