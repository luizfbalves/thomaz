import type { FastifyInstance } from "fastify";
import type { Env } from "../config.js";
import { actionError } from "../lib/errors.js";
import { fetchFeedPage } from "../lib/feed-page.js";
import { userIdFromRequest } from "../plugins/auth.js";

const usernameParamSchema = /^[a-zA-Z0-9_]{3,32}$/;

function toUserProfileDto(user: {
  id: string;
  username: string;
  createdAt: Date;
  _count: { posts: number };
}): {
  id: string;
  username: string;
  createdAt: number;
  postCount: number;
} {
  return {
    id: user.id,
    username: user.username,
    createdAt: Math.floor(user.createdAt.getTime() / 1000),
    postCount: user._count.posts,
  };
}

export async function usersRoutes(
  app: FastifyInstance,
  env: Env,
): Promise<void> {
  app.get(
    "/users/me",
    { preHandler: [app.authenticate] },
    async (request, reply) => {
      const userId = userIdFromRequest(request);
      if (!userId) {
        return reply.status(401).send(actionError("unauthorized"));
      }

      const user = await app.prisma.user.findUnique({
        where: { id: userId },
        include: { _count: { select: { posts: true } } },
      });
      if (!user) {
        return reply.status(404).send(actionError("user_not_found"));
      }

      return toUserProfileDto(user);
    },
  );

  app.get("/users/:username", async (request, reply) => {
    const { username } = request.params as { username: string };
    if (!usernameParamSchema.test(username)) {
      return reply.status(404).send(actionError("user_not_found"));
    }

    const user = await app.prisma.user.findUnique({
      where: { username },
      include: { _count: { select: { posts: true } } },
    });
    if (!user) {
      return reply.status(404).send(actionError("user_not_found"));
    }

    return toUserProfileDto(user);
  });

  app.get(
    "/users/:username/posts",
    { preHandler: [app.optionalAuth] },
    async (request, reply) => {
      const { username } = request.params as { username: string };
      if (!usernameParamSchema.test(username)) {
        return reply.status(404).send(actionError("user_not_found"));
      }

      const user = await app.prisma.user.findUnique({ where: { username } });
      if (!user) {
        return reply.status(404).send(actionError("user_not_found"));
      }

      const cursorRaw =
        typeof request.query === "object" &&
        request.query !== null &&
        "cursor" in request.query
          ? String((request.query as { cursor?: string }).cursor ?? "")
          : "";

      const viewerId = userIdFromRequest(request);
      const result = await fetchFeedPage(app.prisma, env, {
        cursorRaw,
        viewerUserId: viewerId,
        authorId: user.id,
      });

      if (!result.ok) {
        return reply.status(400).send(actionError("invalid_cursor"));
      }

      return result.page;
    },
  );
}
