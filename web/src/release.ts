import { readFileSync } from "node:fs";
import { resolve } from "node:path";

export interface Release {
  schemaVersion: number;
  name: string;
  version: string;
  commit: string;
  configSha256: string;
  chipFamily: string;
  flashSize: number | string;
  config: {
    bluetooth: { name: string };
    gestures: {
      long_press_ms: number;
      double_click_ms: number;
      debounce_ms: number;
    };
    buttons: Record<
      "main" | "secondary",
      Record<"single" | "double" | "long", string | null>
    >;
  };
  parts: { path: string; offset: number; size: number; sha256: string }[];
}

export function readRelease(): Release | null {
  try {
    return JSON.parse(
      readFileSync(
        resolve(process.cwd(), "public/firmware/manifest.json"),
        "utf8",
      ),
    );
  } catch {
    return null;
  }
}

export function keyLabel(key: string | null): string {
  const labels: Record<string, string> = {
    RIGHT: "→ Right arrow",
    LEFT: "← Left arrow",
    ESC: "Escape",
    SPACE: "Space",
    ENTER: "Enter",
  };
  return key ? (labels[key] ?? key.replaceAll("_", " ")) : "Unassigned";
}
