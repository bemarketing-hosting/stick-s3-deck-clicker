import { ESPLoader, Transport } from "esptool-js";
import { md5 } from "@noble/hashes/legacy.js";
import { bytesToHex } from "@noble/hashes/utils.js";
import type { FlashAdapter } from "./session";

export function createSerialAdapter(log: (line: string) => void): FlashAdapter {
  let transport: Transport | undefined;
  let loader: ESPLoader | undefined;
  let selectedPort: SerialPort | undefined;
  let disconnectCallback: (() => void) | undefined;
  const disconnected = (event: Event) => {
    if (event.target === selectedPort) disconnectCallback?.();
  };
  return {
    onDisconnect(callback) {
      disconnectCallback = callback;
    },
    async connect() {
      // Called directly from the button click: keep the browser's user activation.
      const port = await navigator.serial.requestPort();
      selectedPort = port;
      navigator.serial.addEventListener("disconnect", disconnected);
      transport = new Transport(port);
      loader = new ESPLoader({
        transport,
        baudrate: 115200,
        terminal: {
          clean() {},
          write: log,
          writeLine: log,
        },
      });
      await loader.main();
      const flashId = await loader.readFlashId();
      const size = await loader.detectFlashSize();
      return {
        chip: loader.chip.CHIP_NAME,
        flashSize: size === "8MB" ? 8388608 : 0,
        flashId,
      };
    },
    async write(images, eraseAll, progress) {
      if (!loader) throw new Error("Connect the device before installing.");
      const total = images.reduce((sum, image) => sum + image.data.length, 0);
      await loader.writeFlash({
        fileArray: images,
        flashMode: "keep",
        flashFreq: "keep",
        flashSize: "keep",
        eraseAll,
        compress: true,
        calculateMD5Hash: (image) => bytesToHex(md5(image)),
        reportProgress(index, written, fileTotal) {
          const prior = images
            .slice(0, index)
            .reduce((sum, image) => sum + image.data.length, 0);
          progress(
            Math.min(
              99,
              Math.round(
                ((prior + (images[index].data.length * written) / fileTotal) /
                  total) *
                  100,
              ),
            ),
          );
        },
      });
    },
    async reset() {
      await loader?.after("hard_reset");
    },
    async close() {
      const current = transport;
      navigator.serial.removeEventListener("disconnect", disconnected);
      selectedPort = undefined;
      loader = undefined;
      transport = undefined;
      await current?.disconnect();
    },
  };
}
