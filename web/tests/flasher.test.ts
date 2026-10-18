import { test } from "node:test";
import assert from "node:assert/strict";
import {
  loadImages,
  sha256,
  validateManifest,
  type FirmwareManifest,
} from "../src/lib/firmware";
import {
  FlashSession,
  type FlashAdapter,
  type SessionUpdate,
} from "../src/lib/session";

async function fixture(): Promise<FirmwareManifest> {
  const hash = await sha256(new Uint8Array([1, 2, 3, 4]));
  return {
    schemaVersion: 1,
    name: "Clicker",
    version: "test",
    commit: "abcdef",
    configSha256: hash,
    chipFamily: "ESP32-S3",
    flashSize: 8388608,
    config: {},
    parts: [0, 0x8000, 0xe000, 0x10000].map((offset, index) => ({
      path: `${index}.bin`,
      offset,
      size: 4,
      sha256: hash,
    })),
  };
}
function fake(overrides: Partial<FlashAdapter> = {}) {
  const calls: string[] = [];
  const events: SessionUpdate[] = [];
  const adapter: FlashAdapter = {
    async connect() {
      calls.push("connect");
      return { chip: "ESP32-S3", flashSize: 8388608, flashId: 123 };
    },
    async write(_images, erase, progress) {
      calls.push(`write:${erase}`);
      progress(50);
    },
    async reset() {
      calls.push("reset");
    },
    async close() {
      calls.push("close");
    },
    ...overrides,
  };
  return {
    session: new FlashSession(adapter, (event) => events.push(event)),
    calls,
    events,
  };
}

test("manifest validates pinned regions and refuses NVS, duplicate parts, traversal, and oversize", async () => {
  const manifest = await fixture();
  assert.equal(validateManifest(manifest), manifest);
  for (const change of [
    { offset: 0x9000 },
    { offset: 0x8000 },
    { path: "../firmware.bin" },
    { size: 0x9000 },
    { sha256: "bad" },
  ]) {
    const copy = structuredClone(manifest);
    Object.assign(copy.parts[0], change);
    assert.throws(() => validateManifest(copy));
  }
  assert.throws(() =>
    validateManifest({
      ...manifest,
      parts: [null, ...manifest.parts.slice(1)],
    }),
  );
});

test("downloads are relative to manifest and every part requires expected size and SHA256", async () => {
  const manifest = await fixture();
  const urls: string[] = [];
  const fetcher = (async (input: URL | RequestInfo) => {
    urls.push(String(input));
    return new Response(new Uint8Array([1, 2, 3, 4]));
  }) as typeof fetch;
  const images = await loadImages(
    manifest,
    "https://example.org/project/firmware/manifest.json",
    fetcher,
  );
  assert.equal(images.length, 4);
  assert.equal(urls[0], "https://example.org/project/firmware/0.bin");
  await assert.rejects(
    loadImages(
      manifest,
      "https://example.org/manifest.json",
      (async () => new Response(new Uint8Array([4, 3, 2, 1]))) as typeof fetch,
    ),
    /integrity/,
  );
  await assert.rejects(
    loadImages(
      manifest,
      "https://example.org/manifest.json",
      (async () => new Response("", { status: 404 })) as typeof fetch,
    ),
    /download/,
  );
});

test("connection never writes; install requires physical confirmation and defaults can preserve bonds", async () => {
  const { session, calls, events } = fake();
  const manifest = await fixture();
  await session.connect();
  assert.deepEqual(calls, ["connect"]);
  await session.install(manifest, false, false, async () => []);
  assert.deepEqual(calls, ["connect"]);
  await session.install(manifest, true, false, async () => []);
  assert.deepEqual(calls, ["connect", "write:false", "reset", "close"]);
  assert.equal(session.phase, "complete");
  assert.equal(events.at(-1)?.progress, 100);
});

test("wrong chip, wrong flash capacity, and unreadable flash identity cannot reach install", async () => {
  for (const identity of [
    { chip: "ESP32", flashSize: 8388608, flashId: 1 },
    { chip: "ESP32-S3", flashSize: 4194304, flashId: 1 },
    { chip: "ESP32-S3", flashSize: 8388608, flashId: 0 },
  ]) {
    const { session, calls } = fake({
      async connect() {
        return identity;
      },
    });
    await session.connect();
    assert.equal(session.phase, "error");
    assert.deepEqual(calls, ["close"]);
  }
});

test("checksum/download failure closes device without writing or erasing", async () => {
  const { session, calls } = fake();
  await session.connect();
  await session.install(await fixture(), true, true, async () => {
    throw new Error("Bad checksum");
  });
  assert.equal(session.phase, "error");
  assert.deepEqual(calls, ["connect", "close"]);
});

test("write or verification failure never reports completion, releases port, and allows reconnect", async () => {
  const { session, calls, events } = fake({
    async write() {
      throw new Error("MD5 mismatch");
    },
  });
  await session.connect();
  await session.install(await fixture(), true, false, async () => []);
  assert.equal(session.phase, "error");
  assert.ok(!events.some((event) => event.phase === "complete"));
  assert.deepEqual(calls, ["connect", "close"]);
  await session.connect();
  assert.equal(session.phase, "review");
});

test("explicit erase reaches adapter; reset failure after verified write explains manual reset", async () => {
  const { session, calls, events } = fake({
    async reset() {
      throw new Error("USB reset disconnected");
    },
  });
  await session.connect();
  await session.install(await fixture(), true, true, async () => []);
  assert.ok(calls.includes("write:true"));
  assert.equal(session.phase, "complete");
  assert.match(events.at(-1)!.message, /Press the reset button/);
});

test("concurrent install and disconnect cannot interrupt writing", async () => {
  let finish!: () => void;
  const { session, calls } = fake({
    async write() {
      calls.push("write");
      await new Promise<void>((resolve) => {
        finish = resolve;
      });
    },
  });
  const manifest = await fixture();
  await session.connect();
  const installing = session.install(manifest, true, false, async () => []);
  await new Promise((resolve) => setTimeout(resolve, 0));
  await session.install(manifest, true, true, async () => []);
  await session.disconnect();
  assert.deepEqual(calls, ["connect", "write"]);
  finish();
  await installing;
  assert.equal(session.phase, "complete");
});

test("disconnect from review resets chip and closes port", async () => {
  const { session, calls } = fake();
  await session.connect();
  await session.disconnect();
  assert.deepEqual(calls, ["connect", "reset", "close"]);
  assert.equal(session.phase, "idle");
});

test("physical disconnect invalidates review and releases transport before reconnect", async () => {
  let disconnected!: () => void;
  const { session, calls } = fake({
    onDisconnect(callback) {
      disconnected = callback;
    },
  });
  await session.connect();
  disconnected();
  await new Promise((resolve) => setTimeout(resolve, 0));
  assert.equal(session.phase, "error");
  await session.install(await fixture(), true, false, async () => []);
  assert.deepEqual(calls, ["connect", "close"]);
});

test("cancelled port chooser releases resources and permits another connection attempt", async () => {
  const { session, calls } = fake({
    async connect() {
      throw new DOMException("No port selected", "NotFoundError");
    },
  });
  await session.connect();
  assert.equal(session.phase, "error");
  assert.deepEqual(calls, ["close"]);
});
