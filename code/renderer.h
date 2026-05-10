#pragma once
#include "engine_platform.h"

// OpenGL renderer platform-agnostic API

// Software renderer platform-agnostic API
internal void TiledRenderGroupToBuffer(RenderCommandBuffer* commands, LoadedBitmap& dstBuffer, PlatformQueue* queue);
internal void SortRenderCommands(RenderCommandBuffer* commands);
inline void ResetRenderCommands(RenderCommandBuffer* commands);