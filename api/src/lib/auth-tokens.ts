import type { FastifyReply } from "fastify";
import type { Env } from "../config.js";
import type { PrismaClient } from "@prisma/client";
import { issueRefreshToken } from "./refresh-tokens.js";

export async function signAuthResponse(
  reply: FastifyReply,
  prisma: PrismaClient,
  env: Env,
  user: { id: string; username: string },
): Promise<{ ok: true; token: string; refreshToken: string }> {
  const token = await reply.jwtSign(
    { sub: user.id, username: user.username },
    { expiresIn: env.JWT_ACCESS_EXPIRES },
  );
  const refreshToken = await issueRefreshToken(prisma, env, user.id);
  return { ok: true, token, refreshToken };
}
