import { createCipheriv, createDecipheriv, randomBytes } from "node:crypto";

function decodeKey(keyB64: string): Buffer {
  const key = Buffer.from(keyB64, "base64");
  if (key.length !== 32) {
    throw new Error("invalid_key_length");
  }
  return key;
}

/** Encrypt a credential for at-rest storage; returns iv:tag:ciphertext (base64 segments). */
export function encryptSecret(plain: string, keyB64: string): string {
  const key = decodeKey(keyB64);
  const iv = randomBytes(12);
  const cipher = createCipheriv("aes-256-gcm", key, iv);
  const encrypted = Buffer.concat([
    cipher.update(plain, "utf8"),
    cipher.final(),
  ]);
  const tag = cipher.getAuthTag();
  return [
    iv.toString("base64"),
    tag.toString("base64"),
    encrypted.toString("base64"),
  ].join(":");
}

/** Decrypt a credential bundle produced by encryptSecret. */
export function decryptSecret(bundle: string, keyB64: string): string {
  const key = decodeKey(keyB64);
  const parts = bundle.split(":");
  if (parts.length !== 3) {
    throw new Error("invalid_bundle");
  }
  const [ivB64, tagB64, ctB64] = parts;
  const iv = Buffer.from(ivB64, "base64");
  const tag = Buffer.from(tagB64, "base64");
  const ciphertext = Buffer.from(ctB64, "base64");
  const decipher = createDecipheriv("aes-256-gcm", key, iv);
  decipher.setAuthTag(tag);
  return Buffer.concat([
    decipher.update(ciphertext),
    decipher.final(),
  ]).toString("utf8");
}
