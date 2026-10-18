import type { FirmwareImage, FirmwareManifest } from "./firmware";

export type Phase =
  | "idle"
  | "connecting"
  | "review"
  | "downloading"
  | "flashing"
  | "complete"
  | "error";
export interface DeviceIdentity {
  chip: string;
  flashSize: number;
  flashId: number;
}
export interface FlashAdapter {
  connect(): Promise<DeviceIdentity>;
  write(
    images: FirmwareImage[],
    eraseAll: boolean,
    progress: (value: number) => void,
  ): Promise<void>;
  reset(): Promise<void>;
  close(): Promise<void>;
  onDisconnect?(callback: () => void): void;
}
export interface SessionUpdate {
  phase: Phase;
  message: string;
  progress: number;
  device?: DeviceIdentity;
}

export class FlashSession {
  phase: Phase = "idle";
  private device?: DeviceIdentity;
  constructor(
    private adapter: FlashAdapter,
    private emit: (update: SessionUpdate) => void,
  ) {
    adapter.onDisconnect?.(() => {
      // Active I/O owns its error/cleanup path; don't close the transport under it.
      if (this.phase === "review") {
        this.device = undefined;
        this.update(
          "connecting",
          "USB device disconnected. Releasing the connection…",
        );
        void this.fail(
          new Error(
            "USB device disconnected. Reconnect the cable and connect again.",
          ),
        );
      }
    });
  }
  private update(phase: Phase, message: string, progress = 0) {
    this.phase = phase;
    this.emit({ phase, message, progress, device: this.device });
  }
  get busy() {
    return ["connecting", "downloading", "flashing"].includes(this.phase);
  }
  private async cleanup() {
    try {
      await this.adapter.close();
    } catch {
      /* Unplugged ports may already be closed. */
    }
  }
  private async fail(error: unknown) {
    await this.cleanup();
    this.device = undefined;
    this.update(
      "error",
      error instanceof Error ? error.message : "The device connection failed.",
    );
  }
  async connect() {
    if (this.busy || this.phase === "review") return;
    this.device = undefined;
    this.update(
      "connecting",
      "Connecting and checking the chip and flash memory…",
    );
    try {
      const device = await this.adapter.connect();
      if (
        device.chip !== "ESP32-S3" ||
        device.flashSize !== 8388608 ||
        !device.flashId
      ) {
        throw new Error(
          `Incompatible device: ${device.chip}, ${device.flashSize / 1048576} MB flash. This release requires ESP32-S3 with 8 MB flash.`,
        );
      }
      this.device = device;
      this.update(
        "review",
        "Compatible chip detected. Confirm your physical device is a Stick-S3, then install.",
      );
    } catch (error) {
      await this.fail(error);
    }
  }
  async install(
    manifest: FirmwareManifest,
    confirmed: boolean,
    eraseAll: boolean,
    download: () => Promise<FirmwareImage[]>,
  ) {
    if (this.phase !== "review" || !confirmed || !this.device) return;
    if (
      manifest.chipFamily !== this.device.chip ||
      manifest.flashSize !== this.device.flashSize
    ) {
      await this.fail(
        new Error("Firmware does not match the connected device."),
      );
      return;
    }
    this.update(
      "downloading",
      "Downloading and checking every firmware file before writing…",
    );
    try {
      const images = await download();
      this.update(
        "flashing",
        eraseAll
          ? "Erasing saved pairing and installing firmware…"
          : "Installing firmware and verifying flash checksums…",
      );
      await this.adapter.write(images, eraseAll, (progress) =>
        this.update(
          "flashing",
          "Installing firmware and verifying flash checksums…",
          progress,
        ),
      );
      let resetWarning = "";
      try {
        await this.adapter.reset();
      } catch {
        resetWarning = " Press the reset button to start the clicker.";
      }
      await this.cleanup();
      this.update(
        "complete",
        `Firmware installed and flash checksums verified.${resetWarning} Check the display, then pair and test your presentation.`,
        100,
      );
    } catch (error) {
      await this.fail(error);
    }
  }
  async disconnect() {
    if (this.busy) return;
    const needsReset = this.phase === "review";
    this.update("connecting", "Releasing the device connection…");
    try {
      if (needsReset) await this.adapter.reset();
    } catch {
      /* Manual reset remains available. */
    }
    await this.cleanup();
    this.device = undefined;
    this.update("idle", "Disconnected. Connect your Stick-S3 when ready.");
  }
}
