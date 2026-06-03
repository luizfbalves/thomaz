/** Opaque feed cursor: base64url of "createdAtIso|postId" */

export function encodeCursor(createdAt: Date, id: string): string {
  const payload = `${createdAt.toISOString()}|${id}`;
  return Buffer.from(payload, "utf8").toString("base64url");
}

export function decodeCursor(
  cursor: string,
): { createdAt: Date; id: string } | null {
  try {
    const raw = Buffer.from(cursor, "base64url").toString("utf8");
    const sep = raw.lastIndexOf("|");
    if (sep <= 0) return null;
    const iso = raw.slice(0, sep);
    const id = raw.slice(sep + 1);
    const createdAt = new Date(iso);
    if (Number.isNaN(createdAt.getTime()) || !id) return null;
    return { createdAt, id };
  } catch {
    return null;
  }
}
