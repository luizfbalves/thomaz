import type { Comment, Post, SaveSlot, User } from "@prisma/client";
import type { Env } from "../config.js";
import { formatTitleId } from "./title-id.js";

export type { SaveSlot };

export type PostWithCounts = Post & {
  author: User;
  _count: { likes: number; comments: number };
  likes?: { userId: string }[];
};

export function toUserDto(u: User): { id: string; username: string } {
  return { id: u.id, username: u.username };
}

export function toPostDto(
  post: PostWithCounts,
  env: Env,
  viewerUserId?: string,
): {
  id: string;
  author: { id: string; username: string };
  imageUrl: string;
  caption: string;
  gameTitleId: string;
  gameName: string;
  likeCount: number;
  likedByMe: boolean;
  commentCount: number;
  createdAt: number;
} {
  const likedByMe =
    viewerUserId != null &&
    (post.likes?.some((l) => l.userId === viewerUserId) ?? false);

  return {
    id: post.id,
    author: toUserDto(post.author),
    imageUrl: `${env.PUBLIC_BASE_URL}/uploads/${post.imageKey}`,
    caption: post.caption,
    gameTitleId: formatTitleId(post.gameTitleId),
    gameName: post.gameName,
    likeCount: post._count.likes,
    likedByMe,
    commentCount: post._count.comments,
    createdAt: Math.floor(post.createdAt.getTime() / 1000),
  };
}

export function toSaveSlotDto(slot: SaveSlot): {
  titleId: string;
  label: string;
  revision: number;
  updatedAt: number;
  hasData: boolean;
} {
  return {
    titleId: formatTitleId(slot.titleId),
    label: slot.label,
    revision: slot.revision,
    updatedAt: Math.floor(slot.updatedAt.getTime() / 1000),
    hasData: slot.blobKey != null,
  };
}

export function toCommentDto(
  c: Comment & { author: User },
): {
  id: string;
  author: { id: string; username: string };
  text: string;
  createdAt: number;
} {
  return {
    id: c.id,
    author: toUserDto(c.author),
    text: c.text,
    createdAt: Math.floor(c.createdAt.getTime() / 1000),
  };
}
