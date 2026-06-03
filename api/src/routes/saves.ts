import { z } from "zod";
import type { FastifyInstance } from "fastify";
import type { Env } from "../config.js";
import { actionError } from "../lib/errors.js";
import { toSaveSlotDto, type SaveSlot } from "../lib/serializers.js";
import {
  deleteSaveBlob,
  readSaveBlob,
  writeSaveBlob,
} from "../lib/save-storage.js";
import { parseTitleIdParam } from "../lib/title-id.js";
import { userIdFromRequest } from "../plugins/auth.js";

const saveLabelSchema = z.string().max(64).optional();

const savePutFieldsSchema = z.object({
  label: saveLabelSchema,
  revision: z.coerce.number().int().nonnegative().optional(),
});

export async function savesRoutes(
  app: FastifyInstance,
  env: Env,
): Promise<void> {
  app.get(
    "/saves",
    { preHandler: [app.authenticate] },
    async (request, reply) => {
      const userId = userIdFromRequest(request);
      if (!userId) {
        return reply.status(401).send(actionError("unauthorized"));
      }

      const slots = await app.prisma.saveSlot.findMany({
        where: { userId },
        orderBy: { updatedAt: "desc" },
      });

      return { slots: slots.map((s: SaveSlot) => toSaveSlotDto(s)) };
    },
  );

  app.get(
    "/saves/:titleId",
    { preHandler: [app.authenticate] },
    async (request, reply) => {
      const userId = userIdFromRequest(request);
      if (!userId) {
        return reply.status(401).send(actionError("unauthorized"));
      }

      const { titleId: titleIdRaw } = request.params as { titleId: string };
      const titleId = parseTitleIdParam(titleIdRaw);
      if (titleId === null) {
        return reply.status(400).send(actionError("invalid_title_id"));
      }

      const slot = await app.prisma.saveSlot.findUnique({
        where: { userId_titleId: { userId, titleId } },
      });
      if (!slot) {
        return reply.status(404).send(actionError("save_not_found"));
      }

      const includeData =
        typeof request.query === "object" &&
        request.query !== null &&
        "includeData" in request.query &&
        String((request.query as { includeData?: string }).includeData) === "1";

      const base = toSaveSlotDto(slot);
      if (!includeData) {
        return { slot: base };
      }

      if (!slot.blobKey) {
        return reply.status(404).send(actionError("save_data_missing"));
      }

      const data = await readSaveBlob(env, slot.blobKey);
      if (!data) {
        return reply.status(404).send(actionError("save_data_missing"));
      }

      return {
        slot: {
          ...base,
          data: data.toString("base64"),
        },
      };
    },
  );

  app.put(
    "/saves/:titleId",
    { preHandler: [app.authenticate] },
    async (request, reply) => {
      const userId = userIdFromRequest(request);
      if (!userId) {
        return reply.status(401).send(actionError("unauthorized"));
      }

      const { titleId: titleIdRaw } = request.params as { titleId: string };
      const titleId = parseTitleIdParam(titleIdRaw);
      if (titleId === null) {
        return reply.status(400).send(actionError("invalid_title_id"));
      }

      const parts = request.parts();
      let label: string | undefined;
      let revision: number | undefined;
      let saveBuffer: Buffer | null = null;

      for await (const part of parts) {
        if (part.type === "file" && part.fieldname === "data") {
          const chunks: Buffer[] = [];
          for await (const chunk of part.file) {
            chunks.push(chunk);
          }
          saveBuffer = Buffer.concat(chunks);
        } else if (part.type === "field") {
          const value = String(part.value);
          if (part.fieldname === "label") label = value;
          if (part.fieldname === "revision") {
            const n = Number(value);
            if (!Number.isNaN(n)) revision = n;
          }
        }
      }

      if (!saveBuffer) {
        return reply.status(400).send(actionError("missing_save_data"));
      }

      const fieldsParsed = savePutFieldsSchema.safeParse({ label, revision });
      if (!fieldsParsed.success) {
        return reply.status(400).send(actionError("invalid_body"));
      }

      const existing = await app.prisma.saveSlot.findUnique({
        where: { userId_titleId: { userId, titleId } },
      });

      if (existing) {
        const expected = fieldsParsed.data.revision;
        if (expected === undefined) {
          return reply.status(400).send(actionError("revision_required"));
        }
        if (expected !== existing.revision) {
          return reply.status(409).send(actionError("revision_conflict"));
        }
      } else if (fieldsParsed.data.revision !== undefined && fieldsParsed.data.revision !== 0) {
        return reply.status(409).send(actionError("revision_conflict"));
      }

      try {
        const { blobKey } = await writeSaveBlob(
          env,
          userId,
          titleId,
          saveBuffer,
        );

        if (existing?.blobKey && existing.blobKey !== blobKey) {
          await deleteSaveBlob(env, existing.blobKey);
        }

        const nextLabel =
          fieldsParsed.data.label !== undefined
            ? fieldsParsed.data.label
            : (existing?.label ?? "");

        const slot = await app.prisma.saveSlot.upsert({
          where: { userId_titleId: { userId, titleId } },
          create: {
            userId,
            titleId,
            label: nextLabel,
            revision: 1,
            blobKey,
          },
          update: {
            label: nextLabel,
            revision: existing ? existing.revision + 1 : 1,
            blobKey,
          },
        });

        return { ok: true, slot: toSaveSlotDto(slot) };
      } catch (e) {
        const msg = e instanceof Error ? e.message : "upload_failed";
        if (msg === "save_too_large") {
          return reply.status(413).send(actionError("save_too_large"));
        }
        if (msg === "empty_save") {
          return reply.status(400).send(actionError("missing_save_data"));
        }
        return reply.status(500).send(actionError("failed_to_save"));
      }
    },
  );

  app.delete(
    "/saves/:titleId",
    { preHandler: [app.authenticate] },
    async (request, reply) => {
      const userId = userIdFromRequest(request);
      if (!userId) {
        return reply.status(401).send(actionError("unauthorized"));
      }

      const { titleId: titleIdRaw } = request.params as { titleId: string };
      const titleId = parseTitleIdParam(titleIdRaw);
      if (titleId === null) {
        return reply.status(400).send(actionError("invalid_title_id"));
      }

      const slot = await app.prisma.saveSlot.findUnique({
        where: { userId_titleId: { userId, titleId } },
      });
      if (!slot) {
        return reply.status(404).send(actionError("save_not_found"));
      }

      await deleteSaveBlob(env, slot.blobKey);
      await app.prisma.saveSlot.delete({
        where: { userId_titleId: { userId, titleId } },
      });

      return { ok: true };
    },
  );
}
