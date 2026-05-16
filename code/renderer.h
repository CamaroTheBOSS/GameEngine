#pragma once
#include "engine_platform.h"
//
// 
// OpenGL renderer windows specific
#define WGL_CONTEXT_MAJOR_VERSION_ARB           0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB           0x2092
#define WGL_CONTEXT_LAYER_PLANE_ARB             0x2093
#define WGL_CONTEXT_FLAGS_ARB                   0x2094
#define WGL_CONTEXT_PROFILE_MASK_ARB            0x9126
#define WGL_CONTEXT_DEBUG_BIT_ARB               0x0001
#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
#define WGL_DRAW_TO_WINDOW_ARB                  0x2001
#define WGL_ACCELERATION_ARB                    0x2003
#define WGL_SUPPORT_OPENGL_ARB                  0x2010
#define WGL_DOUBLE_BUFFER_ARB                   0x2011
#define WGL_PIXEL_TYPE_ARB                      0x2013
#define WGL_COLOR_BITS_ARB                      0x2014
#define WGL_FULL_ACCELERATION_ARB               0x2027
#define WGL_TYPE_RGBA_ARB                       0x202B

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