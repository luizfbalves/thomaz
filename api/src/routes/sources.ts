import { z } from "zod";
import type { FastifyInstance } from "fastify";
import type { Env } from "../config.js";
import { actionError } from "../lib/errors.js";
import { encryptSecret } from "../lib/source-crypto.js";
import { toSourceLinkDto, type SourceLink } from "../lib/serializers.js";
import { userIdFromRequest } from "../plugins/auth.js";

const authTypeSchema = z.enum(["none", "basicInUrl", "header", "referrer"]);

const sourcePutBodySchema = z.object({
  label: z.string().max(128).optional(),
  url: z.string().url(),
  authType: authTypeSchema,
  secret: z.string().max(4096).optional(),
});

export async function sourcesRoutes(
  app: FastifyInstance,
  env: Env,
): Promise<void> {
  app.get(
    "/sources",
    { preHandler: [app.authenticate] },
    async (request, reply) => {
      const userId = userIdFromRequest(request);
      if (!userId) {
        return reply.status(401).send(actionError("unauthorized"));
      }

      const links = await app.prisma.sourceLink.findMany({
        where: { userId },
        orderBy: { updatedAt: "desc" },
      });

      return { sources: links.map((l: SourceLink) => toSourceLinkDto(l)) };
    },
  );

  app.get(
    "/sources/:id",
    { preHandler: [app.authenticate] },
    async (request, reply) => {
      const userId = userIdFromRequest(request);
      if (!userId) {
        return reply.status(401).send(actionError("unauthorized"));
      }

      const { id } = request.params as { id: string };
      const link = await app.prisma.sourceLink.findFirst({
        where: { userId, id },
      });
      if (!link) {
        return reply.status(404).send(actionError("source_not_found"));
      }

      return { source: toSourceLinkDto(link) };
    },
  );

  app.put(
    "/sources/:id",
    { preHandler: [app.authenticate] },
    async (request, reply) => {
      const userId = userIdFromRequest(request);
      if (!userId) {
        return reply.status(401).send(actionError("unauthorized"));
      }

      const { id } = request.params as { id: string };
      const parsed = sourcePutBodySchema.safeParse(request.body);
      if (!parsed.success) {
        return reply.status(400).send(actionError("invalid_body"));
      }

      const { label, url, authType, secret } = parsed.data;
      const existing = await app.prisma.sourceLink.findFirst({
        where: { userId, id },
      });

      let authSecretEnc: string | null = null;
      if (authType === "none") {
        authSecretEnc = null;
      } else if (secret !== undefined) {
        authSecretEnc = encryptSecret(secret, env.SOURCE_ENC_KEY);
      } else if (existing) {
        authSecretEnc = existing.authSecretEnc;
      } else {
        return reply.status(400).send(actionError("secret_required"));
      }

      if (!existing) {
        const collision = await app.prisma.sourceLink.findUnique({
          where: { id },
        });
        if (collision) {
          return reply.status(404).send(actionError("source_not_found"));
        }
      }

      const nextLabel =
        label !== undefined ? label : (existing?.label ?? "");

      const link = existing
        ? await app.prisma.sourceLink.update({
            where: { id },
            data: {
              label: nextLabel,
              url,
              authType,
              authSecretEnc,
            },
          })
        : await app.prisma.sourceLink.create({
            data: {
              id,
              userId,
              label: nextLabel,
              url,
              authType,
              authSecretEnc,
            },
          });

      return { ok: true, source: toSourceLinkDto(link) };
    },
  );

  app.delete(
    "/sources/:id",
    { preHandler: [app.authenticate] },
    async (request, reply) => {
      const userId = userIdFromRequest(request);
      if (!userId) {
        return reply.status(401).send(actionError("unauthorized"));
      }

      const { id } = request.params as { id: string };
      const link = await app.prisma.sourceLink.findFirst({
        where: { userId, id },
      });
      if (!link) {
        return reply.status(404).send(actionError("source_not_found"));
      }

      await app.prisma.sourceLink.delete({
        where: { id },
      });

      return { ok: true };
    },
  );
}
