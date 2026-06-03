import { createHash, randomBytes } from "node:crypto";
import type { PrismaClient } from "@prisma/client";
import type { Env } from "../config.js";

export function hashRefreshToken(token: string): string {
  return createHash("sha256").update(token).digest("hex");
}

export function generateRefreshToken(): string {
  return randomBytes(32).toString("base64url");
}

function refreshExpiresAt(env: Env): Date {
  const days = env.REFRESH_TOKEN_TTL_DAYS;
  return new Date(Date.now() + days * 24 * 60 * 60 * 1000);
}

export async function issueRefreshToken(
  prisma: PrismaClient,
  env: Env,
  userId: string,
): Promise<string> {
  const token = generateRefreshToken();
  await prisma.refreshToken.create({
    data: {
      userId,
      tokenHash: hashRefreshToken(token),
      expiresAt: refreshExpiresAt(env),
    },
  });
  return token;
}

export async function rotateRefreshToken(
  prisma: PrismaClient,
  env: Env,
  rawToken: string,
): Promise<{ userId: string; refreshToken: string } | null> {
  const tokenHash = hashRefreshToken(rawToken);
  const existing = await prisma.refreshToken.findUnique({
    where: { tokenHash },
  });
  if (!existing || existing.expiresAt < new Date()) {
    if (existing) {
      await prisma.refreshToken.delete({ where: { id: existing.id } });
    }
    return null;
  }

  await prisma.refreshToken.delete({ where: { id: existing.id } });
  const refreshToken = await issueRefreshToken(prisma, env, existing.userId);
  return { userId: existing.userId, refreshToken };
}

export async function revokeRefreshToken(
  prisma: PrismaClient,
  rawToken: string,
): Promise<boolean> {
  const tokenHash = hashRefreshToken(rawToken);
  const existing = await prisma.refreshToken.findUnique({
    where: { tokenHash },
  });
  if (!existing) return false;
  await prisma.refreshToken.delete({ where: { id: existing.id } });
  return true;
}
