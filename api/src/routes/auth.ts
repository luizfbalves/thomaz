import { z } from "zod";
import type { FastifyInstance } from "fastify";
import type { Env } from "../config.js";
import { signAuthResponse } from "../lib/auth-tokens.js";
import { authError } from "../lib/errors.js";
import { hashPassword, verifyPassword } from "../lib/passwords.js";
import {
  revokeRefreshToken,
  rotateRefreshToken,
} from "../lib/refresh-tokens.js";

const credentialsSchema = z.object({
  username: z
    .string()
    .min(3)
    .max(32)
    .regex(/^[a-zA-Z0-9_]+$/),
  password: z.string().min(6).max(128),
});

const refreshBodySchema = z.object({
  refreshToken: z.string().min(1),
});

export async function authRoutes(
  app: FastifyInstance,
  env: Env,
): Promise<void> {
  app.post("/auth/register", async (request, reply) => {
    const parsed = credentialsSchema.safeParse(request.body);
    if (!parsed.success) {
      return reply.status(400).send(authError("invalid_credentials"));
    }
    const { username, password } = parsed.data;

    const existing = await app.prisma.user.findUnique({ where: { username } });
    if (existing) {
      return reply.status(409).send(authError("username_already_exists"));
    }

    const passwordHash = await hashPassword(password);
    const user = await app.prisma.user.create({
      data: { username, passwordHash },
    });

    return signAuthResponse(reply, app.prisma, env, user);
  });

  app.post("/auth/login", async (request, reply) => {
    const parsed = credentialsSchema.safeParse(request.body);
    if (!parsed.success) {
      return reply.status(400).send(authError("invalid_credentials"));
    }
    const { username, password } = parsed.data;

    const user = await app.prisma.user.findUnique({ where: { username } });
    if (!user || !(await verifyPassword(user.passwordHash, password))) {
      return reply.status(401).send(authError("invalid_credentials"));
    }

    return signAuthResponse(reply, app.prisma, env, user);
  });

  app.post("/auth/refresh", async (request, reply) => {
    const parsed = refreshBodySchema.safeParse(request.body);
    if (!parsed.success) {
      return reply.status(400).send(authError("invalid_body"));
    }

    const rotated = await rotateRefreshToken(
      app.prisma,
      env,
      parsed.data.refreshToken,
    );
    if (!rotated) {
      return reply.status(401).send(authError("invalid_refresh_token"));
    }

    const user = await app.prisma.user.findUnique({
      where: { id: rotated.userId },
    });
    if (!user) {
      return reply.status(401).send(authError("invalid_refresh_token"));
    }

    const token = await reply.jwtSign(
      { sub: user.id, username: user.username },
      { expiresIn: env.JWT_ACCESS_EXPIRES },
    );

    return { ok: true, token, refreshToken: rotated.refreshToken };
  });

  app.post("/auth/logout", async (request, reply) => {
    const parsed = refreshBodySchema.safeParse(request.body);
    if (!parsed.success) {
      return reply.status(400).send(authError("invalid_body"));
    }

    await revokeRefreshToken(app.prisma, parsed.data.refreshToken);
    return { ok: true };
  });
}
