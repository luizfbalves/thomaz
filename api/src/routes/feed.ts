import type { FastifyInstance } from "fastify";
import type { Env } from "../config.js";
import { actionError } from "../lib/errors.js";
import { fetchFeedPage } from "../lib/feed-page.js";
import { userIdFromRequest } from "../plugins/auth.js";

export async function feedRoutes(app: FastifyInstance, env: Env): Promise<void> {
  app.get(
    "/feed",
    { preHandler: [app.optionalAuth] },
    async (request, reply) => {
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
      });

      if (!result.ok) {
        return reply.status(400).send(actionError("invalid_cursor"));
      }

      return result.page;
    },
  );
}
