import type { PrismaClient } from "@prisma/client";
import type { Env } from "../config.js";
import { decodeCursor, encodeCursor } from "./cursor.js";
import { toPostDto, type PostWithCounts } from "./serializers.js";

const PAGE_SIZE = 20;

export type FeedPageResult = {
  posts: ReturnType<typeof toPostDto>[];
  nextCursor: string;
  hasMore: boolean;
};

export async function fetchFeedPage(
  prisma: PrismaClient,
  env: Env,
  options: {
    cursorRaw: string;
    viewerUserId?: string;
    authorId?: string;
  },
): Promise<
  | { ok: true; page: FeedPageResult }
  | { ok: false; error: "invalid_cursor" }
> {
  const cursor = options.cursorRaw ? decodeCursor(options.cursorRaw) : null;
  if (options.cursorRaw && !cursor) {
    return { ok: false, error: "invalid_cursor" };
  }

  const posts = await prisma.post.findMany({
    take: PAGE_SIZE + 1,
    orderBy: [{ createdAt: "desc" }, { id: "desc" }],
    where: {
      ...(options.authorId ? { authorId: options.authorId } : {}),
      ...(cursor
        ? {
            OR: [
              { createdAt: { lt: cursor.createdAt } },
              {
                createdAt: cursor.createdAt,
                id: { lt: cursor.id },
              },
            ],
          }
        : {}),
    },
    include: {
      author: true,
      _count: { select: { likes: true, comments: true } },
      ...(options.viewerUserId
        ? {
            likes: {
              where: { userId: options.viewerUserId },
              select: { userId: true },
            },
          }
        : {}),
    },
  });

  const hasMore = posts.length > PAGE_SIZE;
  const page = hasMore ? posts.slice(0, PAGE_SIZE) : posts;
  const last = page[page.length - 1];
  const nextCursor =
    hasMore && last ? encodeCursor(last.createdAt, last.id) : "";

  return {
    ok: true,
    page: {
      posts: page.map((p: PostWithCounts) =>
        toPostDto(p, env, options.viewerUserId),
      ),
      nextCursor,
      hasMore,
    },
  };
}
