import { mkdir, readFile, unlink, writeFile } from "node:fs/promises";
import { join } from "node:path";
import type { Env } from "../config.js";

export const MAX_SAVE_BYTES = 16 * 1024 * 1024;

function savesDir(env: Env): string {
  return join(env.UPLOAD_DIR, "saves");
}

function blobRelKey(userId: string, titleId: bigint): string {
  return join(userId, `${titleId.toString()}.bin`);
}

export function saveBlobKey(userId: string, titleId: bigint): string {
  return `saves/${blobRelKey(userId, titleId)}`;
}

export async function writeSaveBlob(
  env: Env,
  userId: string,
  titleId: bigint,
  data: Buffer,
): Promise<{ blobKey: string }> {
  if (data.length === 0) throw new Error("empty_save");
  if (data.length > MAX_SAVE_BYTES) throw new Error("save_too_large");

  const blobKey = saveBlobKey(userId, titleId);
  const dir = join(savesDir(env), userId);
  await mkdir(dir, { recursive: true });
  const path = join(savesDir(env), blobRelKey(userId, titleId));
  await writeFile(path, data);
  return { blobKey };
}

export async function readSaveBlob(
  env: Env,
  blobKey: string,
): Promise<Buffer | null> {
  const path = join(env.UPLOAD_DIR, blobKey);
  try {
    return await readFile(path);
  } catch (e) {
    if (
      e &&
      typeof e === "object" &&
      "code" in e &&
      (e as NodeJS.ErrnoException).code === "ENOENT"
    ) {
      return null;
    }
    throw e;
  }
}

export async function deleteSaveBlob(
  env: Env,
  blobKey: string | null,
): Promise<void> {
  if (!blobKey) return;
  const path = join(env.UPLOAD_DIR, blobKey);
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
