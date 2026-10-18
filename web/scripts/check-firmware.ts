import { readFile } from "node:fs/promises";
import { createHash } from "node:crypto";
import { validateManifest } from "../src/lib/firmware";

try {
  const base = new URL("../public/firmware/", import.meta.url);
  const manifest = validateManifest(
    JSON.parse(await readFile(new URL("manifest.json", base), "utf8")),
  );
  for (const part of manifest.parts) {
    const bytes = await readFile(new URL(part.path, base));
    const hash = createHash("sha256").update(bytes).digest("hex");
    if (bytes.length !== part.size || hash !== part.sha256)
      throw new Error(`Size or checksum mismatch: ${part.path}`);
  }
  console.log(
    `Firmware package verified: ${manifest.version} (${manifest.parts.length} parts)`,
  );
} catch (error) {
  console.error(
    "A valid firmware package is required before building the site. Run the repository firmware packaging command first.",
  );
  console.error(error instanceof Error ? error.message : error);
  process.exit(1);
}
