/** Canonical hex title id for JSON (16 uppercase nybbles). */
export function formatTitleId(titleId: bigint): string {
  return titleId.toString(16).toUpperCase().padStart(16, "0");
}

/** Parse Switch title id route/query values (16-digit hex or decimal u64). */
export function parseTitleIdParam(raw: string): bigint | null {
  const trimmed = raw.trim();
  if (!trimmed) return null;

  try {
    if (/^[0-9a-fA-F]{16}$/.test(trimmed)) {
      return BigInt(`0x${trimmed}`);
    }
    if (/^[0-9a-fA-F]+$/i.test(trimmed) && /[a-fA-F]/.test(trimmed)) {
      return BigInt(`0x${trimmed}`);
    }
    if (/^\d+$/.test(trimmed)) {
      const value = BigInt(trimmed);
      return value < 0n ? null : value;
    }
    return null;
  } catch {
    return null;
  }
}
