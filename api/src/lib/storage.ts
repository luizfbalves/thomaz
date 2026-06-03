import { mkdir, unlink, writeFile } from "node:fs/promises";
import { join } from "node:path";
import { randomUUID } from "node:crypto";
import type { Env } from "../config.js";

const MAX_JPEG_BYTES = 5 * 1024 * 1024;

export async function ensureUploadDir(env: Env): Promise<void> {
  await mkdir(env.UPLOAD_DIR, { recursive: true });
}

export async function saveJpeg(
  env: Env,
  data: Buffer,
): Promise<{ imageKey: string }> {
  if (data.length === 0) throw new Error("empty_image");
  if (data.length > MAX_JPEG_BYTES) throw new Error("image_too_large");

  const imageKey = `${randomUUID()}.jpg`;
  const path = join(env.UPLOAD_DIR, imageKey);
  await writeFile(path, data);
  return { imageKey };
}

export async function deleteJpeg(env: Env, imageKey: string): Promise<void> {
  const path = join(env.UPLOAD_DIR, imageKey);
  try {
    await unlink(path);
  } catch (e) {
    if (
      e &&
      typeof e === "object" &&
      "code" in e &&
      (e as NodeJS.ErrnoException).code === "ENOENT"
    ) {
      return;
    }
    throw e;
  }
}

export { MAX_JPEG_BYTES };
