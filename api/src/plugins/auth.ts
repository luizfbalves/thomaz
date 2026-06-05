import type { FastifyInstance, FastifyRequest } from "fastify";
import type { Env } from "../config.js";

export type JwtPayload = {
  sub: string;
  username: string;
  jti?: string;
  exp?: number;
};

declare module "@fastify/jwt" {
  interface FastifyJWT {
    payload: JwtPayload;
    user: JwtPayload;
  }
}

declare module "fastify" {
  interface FastifyInstance {
    authenticate: (
      request: FastifyRequest,
      reply: import("fastify").FastifyReply,
    ) => Promise<void>;
    optionalAuth: (
      request: FastifyRequest,
      reply: import("fastify").FastifyReply,
    ) => Promise<void>;
  }
}

export async function registerAuth(
  app: FastifyInstance,
  env: Env,
): Promise<void> {
  await app.register(import("@fastify/jwt"), {
    secret: env.JWT_SECRET,
  });

  app.decorate(
    "authenticate",
    async function (request, reply) {
      try {
        await request.jwtVerify();
      } catch {
        return reply.status(401).send({ ok: false, error: "unauthorized" });
      }
      const { jti } = request.user as JwtPayload;
      if (!jti) return; // D-05/L-02: pre-deploy token, allow with no DB hit
      try {
        const revoked = await app.prisma.revokedToken.findUnique({
          where: { jti },
        });
        if (revoked) {
          return reply
            .status(401)
            .send({ ok: false, error: "unauthorized" }); // D-03 generic
        }
      } catch (err) {
        request.log.warn({ err }, "revocation lookup failed; allowing"); // D-06 fail-open
      }
    },
  );

  app.decorate("optionalAuth", async function (request, _reply) {
    try {
      await request.jwtVerify();
    } catch {
      // public feed: no user
    }
  });
}

export function userIdFromRequest(
  request: FastifyRequest,
): string | undefined {
  const user = request.user as JwtPayload | undefined;
  return user?.sub;
}
