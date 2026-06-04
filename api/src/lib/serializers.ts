import type { SaveSlot } from "@prisma/client";
import { formatTitleId } from "./title-id.js";

export type { SaveSlot };

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
