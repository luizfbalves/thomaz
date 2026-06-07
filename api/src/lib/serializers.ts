import type { SaveSlot, SourceLink } from "@prisma/client";
import { formatTitleId } from "./title-id.js";

export type { SaveSlot, SourceLink };

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

export function toSourceLinkDto(link: SourceLink): {
  id: string;
  label: string;
  url: string;
  authType: string;
  hasSecret: boolean;
  updatedAt: number;
} {
  return {
    id: link.id,
    label: link.label,
    url: link.url,
    authType: link.authType,
    hasSecret: link.authSecretEnc != null,
    updatedAt: Math.floor(link.updatedAt.getTime() / 1000),
  };
}
