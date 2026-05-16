#include "renderer.h"

#ifndef WINGDIAPI
#define WINGDIAPI
#endif
#ifndef APIENTRY
#define APIENTRY
#endif
#include "gl/GL.h"

static u32 globalTextureIndex = 0;

internal
OpenGLInfo OpenGLInit() {
	OpenGLInfo info = {};
	info.defaultInternalTextureFormat = GL_RGBA8;
	info.vendor = ptrcast(const char, glGetString(GL_VENDOR));
	info.renderer = ptrcast(const char, glGetString(GL_RENDERER));
	info.version = ptrcast(const char, glGetString(GL_VERSION));
	info.shadingLangVersion = ptrcast(const char, glGetString(GL_SHADING_LANGUAGE_VERSION));
	const char* extensions = ptrcast(const char, glGetString(GL_EXTENSIONS));
	const char* at = extensions;

	String8 exts[] = {
		String8FromNullTerminated("GL_EXT_texture_sRGB"),
		String8FromNullTerminated("GL_EXT_framebuffer_sRGB"),
		String8FromNullTerminated("WGL_ARB_create_context"),
	};
	while (*at) {
		while (IsWhiteSpace(*at)) { at++; }
		if (!(*at)) { break; }
		u32 length = StringLengthWhiteSpaceTerminator(at);
		if (StringsAreEqual(at, length, exts[0].str, exts[0].length)) { info.GL_EXT_texture_sRGB = true; }
		else if (StringsAreEqual(at, length, exts[1].str, exts[1].length)) { info.GL_EXT_framebuffer_sRGB = true; }
		else if (StringsAreEqual(at, length, exts[2].str, exts[2].length)) { info.WGL_ARB_create_context = true; }
		at += length;
	}
	if (info.GL_EXT_texture_sRGB) {
		info.defaultInternalTextureFormat = GL_SRGB8_ALPHA8_EXT;
	}
	if (info.GL_EXT_framebuffer_sRGB) {
		glEnable(GL_FRAMEBUFFER_SRGB_EXT);
	}
	return info;
}

inline
void OpenGLRenderRectangle(V2 min, V2 max, V4 color) {
	glColor4f(color.R, color.G, color.B, color.A);

	glBegin(GL_TRIANGLES);
	glTexCoord2f(0.f, 0.f);
	glVertex2f(min.X, min.Y);
	glTexCoord2f(1.f, 0.f);
	glVertex2f(max.X, min.Y);
	glTexCoord2f(1.f, 1.f);
	glVertex2f(max.X, max.Y);

	glTexCoord2f(0.f, 0.f);
	glVertex2f(min.X, min.Y);
	glTexCoord2f(0.f, 1.f);
	glVertex2f(min.X, max.Y);
	glTexCoord2f(1.f, 1.f);
	glVertex2f(max.X, max.Y);
	glEnd();
}

internal
void OpenGLRenderCommandsToBuffer(RenderCommandBuffer* commands, 
	u32 dstOffsetX, u32 dstOffsetY, u32 dstWidth, u32 dstHeight,
	u32 srcWidth, u32 srcHeight, OpenGLInfo& info
) {
	TIMED_FUNCTION;
	glViewport(dstOffsetX, dstOffsetY, dstWidth, dstHeight);

	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glMatrixMode(GL_PROJECTION_MATRIX);
	f32 a = 2.f / srcWidth;
	f32 b = 2.f / srcHeight;
	f32 projectionMatrix[] = {
		a,  0, 0, 0,
		0,  b, 0, 0,
		0,  0, 1, 0,
		-1,-1, 0, 1
	};
	glLoadMatrixf(projectionMatrix);

	
	SortElement* sortElement = ptrcast(SortElement, commands->pushBuffer + commands->sortBufferAt);
	for (u32 sortIndex = 0; sortIndex < commands->pushBufferCount; sortIndex++, sortElement++) {
		u8* address = commands->pushBuffer + sortElement->offset;
		RenderCallHeader* header = ptrcast(RenderCallHeader, address);
		address += sizeof(RenderCallHeader);
		switch (header->type) {
		case RenderCallType_RenderCallClear: {
			RenderCallClear* call = ptrcast(RenderCallClear, address);
			glClearColor(call->color.R, call->color.G, call->color.B, call->color.A);
			glClear(GL_COLOR_BUFFER_BIT);
		} break;
		case RenderCallType_RenderCallRectangle: {
			RenderCallRectangle* call = ptrcast(RenderCallRectangle, address);

			V2 xAxis = V2{ call->size.X, 0 };
			V2 yAxis = V2{ 0, call->size.Y };
			V2 origin = call->center - call->size / 2.f;
			glDisable(GL_TEXTURE_2D);
			OpenGLRenderRectangle(origin, origin + call->size, call->color);
			glEnable(GL_TEXTURE_2D);
		} break;
		case RenderCallType_RenderCallBitmap: {
			// TODO: RenderCallBitmap and RenderCallRectangle have different approaches to calculate center
			// It should be unified (check groundLevel) which is different from RenderCallRectangle,
			// also, size is properly changed in RenderCallRectangle and not in RenderCallBitmap
			RenderCallBitmap* call = ptrcast(RenderCallBitmap, address);
			V2 xAxis = V2{ call->size.X, 0 };
			V2 yAxis = V2{ 0, call->size.Y };
			V2 origin = call->center - Hadamard(call->bitmap->align, call->size);

			LoadedBitmap* bitmap = call->bitmap;
			if (bitmap->glTextureIndex == 0) {
				bitmap->glTextureIndex = ++globalTextureIndex;
				glGenTextures(1, &bitmap->glTextureIndex);
				glBindTexture(GL_TEXTURE_2D, bitmap->glTextureIndex);
				glTexImage2D(GL_TEXTURE_2D, 0, info.defaultInternalTextureFormat, bitmap->width, bitmap->height, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, bitmap->data);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			}
			else {
				glBindTexture(GL_TEXTURE_2D, bitmap->glTextureIndex);
			}
			OpenGLRenderRectangle(origin, origin + call->size, call->color);
		} break;
		InvalidDefaultCase;
		}
	}
}