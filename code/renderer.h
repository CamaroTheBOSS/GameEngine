#pragma once
#include "engine_platform.h"
//
// 
// OpenGL renderer platform-agnostic API
#define GL_SHADING_LANGUAGE_VERSION       0x8B8C
#define GL_SRGB8_ALPHA8_EXT               0x8C43
#define GL_FRAMEBUFFER_SRGB_EXT           0x8DB9

struct OpenGLInfo {
	const char* vendor;
	const char* renderer;
	const char* version;
	const char* shadingLangVersion;
	u32 defaultInternalTextureFormat;

	bool GL_EXT_texture_sRGB;
	bool GL_EXT_framebuffer_sRGB;

	bool WGL_ARB_create_context;
};

internal OpenGLInfo OpenGLInit();
internal void OpenGLRenderCommandsToBuffer(RenderCommandBuffer* commands, 
	u32 dstOffsetX, u32 dstOffsetY, u32 dstWidth, u32 dstHeight,
	u32 srcWidth, u32 srcHeight, OpenGLInfo& info);
//
//
// Software renderer platform-agnostic API
internal void TiledRenderGroupToBuffer(RenderCommandBuffer* commands, LoadedBitmap& dstBuffer, PlatformQueue* queue);
internal void SortRenderCommands(RenderCommandBuffer* commands);
inline void ResetRenderCommands(RenderCommandBuffer* commands);