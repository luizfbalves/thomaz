import { z } from "zod";
import type { FastifyInstance } from "fastify";
import type { Env } from "../config.js";
import { actionError } from "../lib/errors.js";
import { deleteJpeg, saveJpeg } from "../lib/storage.js";
import { parseTitleIdParam } from "../lib/title-id.js";
import { toCommentDto, toPostDto } from "../lib/serializers.js";
import { userIdFromRequest } from "../plugins/auth.js";

const likeBodySchema = z.object({
  liked: z.boolean(),
});

const commentBodySchema = z.object({
  text: z.string().min(1).max(2000),
});

export async function postsRoutes(
  app: FastifyInstance,
  env: Env,
): Promise<void> {
  app.post(
    "/posts",
    { preHandler: [app.authenticate] },
    async (request, reply) => {
      const userId = userIdFromRequest(request);
      if (!userId) {
        return reply.status(401).send(actionError("unauthorized"));
      }

      const parts = request.parts();
      let caption = "";
      let gameTitleId = BigInt(0);
      let gameName = "";
      let imageBuffer: Buffer | null = null;
      let imageMime = "";

      for await (const part of parts) {
        if (part.type === "file" && part.fieldname === "image") {
          imageMime = part.mimetype;
          const chunks: Buffer[] = [];
          for await (const chunk of part.file) {
            chunks.push(chunk);
          }
          imageBuffer = Buffer.concat(chunks);
        } else if (part.type === "field") {
          const value = String(part.value);
          if (part.fieldname === "caption") caption = value;
          if (part.fieldname === "gameTitleId") {
            const parsed = parseTitleIdParam(value);
            gameTitleId = parsed ?? BigInt(0);
          }
          if (part.fieldname === "gameName") gameName = value;
        }
      }

      if (!imageBuffer) {
        return reply.status(400).send(actionError("missing_image"));
      }
      if (imageMime !== "image/jpeg" && imageMime !== "image/jpg") {
        return reply.status(400).send(actionError("invalid_image_type"));
      }

      try {
        const { imageKey } = await saveJpeg(env, imageBuffer);
        const post = await app.prisma.post.create({
          data: {
            authorId: userId,
            caption,
            gameTitleId,
            gameName,
            imageKey,
          },
          include: {
            author: true,
            _count: { select: { likes: true, comments: true } },
            likes: { where: { userId }, select: { userId: true } },
          },
        });

        return { ok: true, post: toPostDto(post, env, userId) };
      } catch (e) {
        const msg = e instanceof Error ? e.message : "upload_failed";
        if (msg === "image_too_large") {
          return reply.status(413).send(actionError("image_too_large"));
        }
        return reply.status(500).send(actionError("failed_to_post"));
      }
    },
  );

  app.put(
    "/posts/:postId/like",
    { preHandler: [app.authenticate] },
    async (request, reply) => {
      const userId = userIdFromRequest(request);
      if (!userId) {
        return reply.status(401).send(actionError("unauthorized"));
      }

      const { postId } = request.params as { postId: string };
      const parsed = likeBodySchema.safeParse(request.body);
      if (!parsed.success) {
        return reply.status(400).send(actionError("invalid_body"));
      }

      const post = await app.prisma.post.findUnique({ where: { id: postId } });
      if (!post) {
        return reply.status(404).send(actionError("post_not_found"));
      }

      if (parsed.data.liked) {
        await app.prisma.like.upsert({
          where: { userId_postId: { userId, postId } },
          create: { userId, postId },
          update: {},
        });
      } else {
        await app.prisma.like.deleteMany({ where: { userId, postId } });
      }

      return { ok: true };
    },
  );

  app.get("/posts/:postId/comments", async (request, reply) => {
    const { postId } = request.params as { postId: string };
    const post = await app.prisma.post.findUnique({ where: { id: postId } });
    if (!post) {
      return reply.status(404).send(actionError("post_not_found"));
    }

    const comments = await app.prisma.comment.findMany({
      where: { postId },
      orderBy: { createdAt: "asc" },
      include: { author: true },
    });

    return comments.map((c: Parameters<typeof toCommentDto>[0]) =>
      toCommentDto(c),
    );
  });

  app.post(
    "/posts/:postId/comments",
    { preHandler: [app.authenticate] },
    async (request, reply) => {
      const userId = userIdFromRequest(request);
      if (!userId) {
        return reply.status(401).send(actionError("unauthorized"));
      }

      const { postId } = request.params as { postId: string };
      const parsed = commentBodySchema.safeParse(request.body);
      if (!parsed.success) {
        return reply.status(400).send(actionError("invalid_body"));
      }

      const post = await app.prisma.post.findUnique({ where: { id: postId } });
      if (!post) {
        return reply.status(404).send(actionError("post_not_found"));
      }

      await app.prisma.comment.create({
        data: {
          postId,
          authorId: userId,
          text: parsed.data.text,
        },
      });

      return { ok: true };
    },
  );

  app.delete(
    "/posts/:postId",
    { preHandler: [app.authenticate] },
    async (request, reply) => {
      const userId = userIdFromRequest(request);
      if (!userId) {
        return reply.status(401).send(actionError("unauthorized"));
      }

      const { postId } = request.params as { postId: string };
      const post = await app.prisma.post.findUnique({ where: { id: postId } });
      if (!post) {
        return reply.status(404).send(actionError("post_not_found"));
      }
      if (post.authorId !== userId) {
        return reply.status(403).send(actionError("forbidden"));
      }

      await app.prisma.post.delete({ where: { id: postId } });
      await deleteJpeg(env, post.imageKey);

      return { ok: true };
    },
  );
}
