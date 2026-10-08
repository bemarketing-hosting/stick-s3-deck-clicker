import {
  loadImages,
  validateManifest,
  type FirmwareManifest,
} from "./firmware";
import { createSerialAdapter } from "./serial-adapter";
import { FlashSession } from "./session";

export function initializeFlasher(
  root: HTMLElement,
  manifestUrl: string,
): void {
  if (root.dataset.initialized) return;
  root.dataset.initialized = "true";
  const get = <T extends HTMLElement>(name: string) => {
    const element = root.querySelector<T>(`[data-${name}]`);
    if (!element) throw new Error(`Missing flasher element: ${name}`);
    return element;
  };
  const connect = get<HTMLButtonElement>("connect");
  const install = get<HTMLButtonElement>("install");
  const disconnect = get<HTMLButtonElement>("disconnect");
  const confirm = get<HTMLInputElement>("confirm");
  const erase = get<HTMLInputElement>("erase");
  const status = get("status");
  const device = get("device");
  const progress = get<HTMLProgressElement>("progress");
  const log = get("log");
  let manifest: FirmwareManifest | undefined;
  const url = new URL(manifestUrl, location.href).href;
  const supported = window.isSecureContext && "serial" in navigator;
  const session = new FlashSession(
    createSerialAdapter((line) => {
      log.textContent = ((log.textContent ?? "") + line + "\n").slice(-24000);
      log.scrollTop = log.scrollHeight;
    }),
    (update) => {
      root.dataset.state = update.phase;
      status.textContent = update.message;
      progress.value = update.progress;
      device.textContent = update.device
        ? `${update.device.chip} · 8 MB · Flash ID ${update.device.flashId.toString(16).toUpperCase()}`
        : "No verified device connected";
      if (update.phase !== "review") confirm.checked = false;
      refresh();
    },
  );
  function refresh() {
    connect.disabled =
      !supported || !manifest || session.busy || session.phase === "review";
    install.disabled =
      session.phase !== "review" || !confirm.checked || !manifest;
    disconnect.disabled = session.phase !== "review";
    confirm.disabled = session.phase !== "review";
    erase.disabled = session.busy;
  }
  root.dataset.state = supported ? "idle" : "unsupported";
  status.textContent = supported
    ? "Loading firmware release…"
    : "Use a desktop browser with Web Serial, such as Chrome or Edge, over HTTPS (or localhost). You can still download the firmware and use the command-line instructions.";
  refresh();
  confirm.addEventListener("change", refresh);
  connect.addEventListener("click", () => {
    log.textContent = "";
    void session.connect();
  });
  disconnect.addEventListener("click", () => {
    void session.disconnect();
  });
  install.addEventListener("click", () => {
    if (manifest)
      void session.install(manifest, confirm.checked, erase.checked, () =>
        loadImages(manifest!, url),
      );
  });
  window.addEventListener("beforeunload", (event) => {
    if (session.busy) {
      event.preventDefault();
      // Older Chromium versions require returnValue as well as preventDefault.
      event.returnValue = "";
    }
  });
  void fetch(url, { cache: "no-store" })
    .then(async (response) => {
      if (!response.ok)
        throw new Error(
          `Firmware release is unavailable (${response.status}).`,
        );
      const fetched = validateManifest(await response.json());
      const expected = root.dataset;
      if (
        (expected.expectedCommit &&
          expected.expectedCommit !== fetched.commit) ||
        (expected.expectedConfigSha256 &&
          expected.expectedConfigSha256 !== fetched.configSha256) ||
        (expected.expectedVersion &&
          expected.expectedVersion !== fetched.version)
      ) {
        throw new Error(
          "A newer firmware release was published after this page loaded. Reload the page before installing.",
        );
      }
      manifest = fetched;
      const version = root.querySelector("[data-version]");
      const config = root.querySelector("[data-config]");
      if (version)
        version.textContent = `${manifest.version} · ${manifest.commit.slice(0, 7)}`;
      if (config) config.textContent = JSON.stringify(manifest.config, null, 2);
      if (supported)
        status.textContent =
          "Ready. Connect your Stick-S3 with a USB data cable.";
      refresh();
    })
    .catch((error) => {
      if (supported) {
        root.dataset.state = "error";
        status.textContent = error.message;
      }
      refresh();
    });
}
