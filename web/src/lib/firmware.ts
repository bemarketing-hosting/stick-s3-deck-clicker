export interface FirmwarePart {
  path: string;
  offset: number;
  size: number;
  sha256: string;
}
export interface FirmwareManifest {
  schemaVersion: 1;
  name: string;
  version: string;
  commit: string;
  configSha256: string;
  chipFamily: "ESP32-S3";
  flashSize: 8388608;
  config: Record<string, unknown>;
  parts: FirmwarePart[];
}
export interface FirmwareImage {
  data: Uint8Array;
  address: number;
}
const hashPattern = /^[a-f0-9]{64}$/;

export function validateManifest(value: unknown): FirmwareManifest {
  if (!value || typeof value !== "object")
    throw new Error("Invalid firmware manifest.");
  const m = value as FirmwareManifest;
  if (
    m.schemaVersion !== 1 ||
    m.chipFamily !== "ESP32-S3" ||
    m.flashSize !== 8388608 ||
    ![m.name, m.version, m.commit].every(
      (v) => typeof v === "string" && v.length > 0,
    ) ||
    !hashPattern.test(m.configSha256) ||
    !m.config ||
    typeof m.config !== "object" ||
    Array.isArray(m.config) ||
    !Array.isArray(m.parts) ||
    m.parts.length !== 4
  )
    throw new Error("Unsupported firmware manifest.");
  // These are the pinned PlatformIO default_8MB image regions. Never write through NVS.
  const regions = new Map([
    [0, 0x8000],
    [0x8000, 0x9000],
    [0xe000, 0x10000],
    [0x10000, 0x340000],
  ]);
  const seen = new Set<number>();
  for (const p of m.parts) {
    if (!p || typeof p !== "object") throw new Error("Invalid firmware part.");
    const end = regions.get(p.offset);
    if (
      !p ||
      typeof p.path !== "string" ||
      !/^[\w./-]+\.bin$/.test(p.path) ||
      p.path.startsWith("/") ||
      p.path.split("/").includes("..") ||
      !Number.isSafeInteger(p.size) ||
      p.size <= 0 ||
      end === undefined ||
      p.offset + p.size > end ||
      seen.has(p.offset) ||
      !hashPattern.test(p.sha256)
    ) {
      throw new Error("Unsafe firmware image layout or checksum.");
    }
    seen.add(p.offset);
  }
  return m;
}

export async function sha256(data: Uint8Array): Promise<string> {
  const digest = await crypto.subtle.digest("SHA-256", new Uint8Array(data));
  return Array.from(new Uint8Array(digest), (b) =>
    b.toString(16).padStart(2, "0"),
  ).join("");
}

export async function loadImages(
  manifest: FirmwareManifest,
  manifestUrl: string,
  fetcher: typeof fetch = fetch,
): Promise<FirmwareImage[]> {
  return Promise.all(
    manifest.parts.map(async (part) => {
      const response = await fetcher(new URL(part.path, manifestUrl), {
        cache: "no-store",
      });
      if (!response.ok)
        throw new Error(
          `Could not download ${part.path} (${response.status}).`,
        );
      const data = new Uint8Array(await response.arrayBuffer());
      if (data.length !== part.size || (await sha256(data)) !== part.sha256) {
        throw new Error(
          `Firmware integrity check failed for ${part.path}. Nothing was written.`,
        );
      }
      return { data, address: part.offset };
    }),
  );
}
