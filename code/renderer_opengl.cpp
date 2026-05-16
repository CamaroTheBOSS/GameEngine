#include "renderer.h"

#ifndef WINGDIAPI
#define WINGDIAPI
#endif
#ifndef APIENTRY
#define APIENTRY
#endif
#include "gl/GL.h"

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
	while (*at) {
		while (IsWhiteSpace(*at)) { at++; }
		u32 length = 0;
		const char* end = at;
		while (*end && !IsWhiteSpace(*end++)) { length++; }
		if (StringsAreEqual(at, length, "GL_EXT_texture_sRGB")) { info.GL_EXT_texture_sRGB = true; }
		else if (StringsAreEqual(at, length, "GL_EXT_framebuffer_sRGB")) { info.GL_EXT_framebuffer_sRGB = true; }
		at += length;
	}
	if (info.GL_EXT_texture_sRGB) {
		info.defaultInternalTextureFormat = GL_SRGB8_ALPHA8_EXT;
	}
	if (info.GL_EXT_framebuffer_sRGB) {
		glEnable(GL_FRAMEBUFFER_SRGB_EXT);
	}
	info.initialized = true;
	return info;
}

inline 
GLuint OpenGLAllocateTexture(void* data, u32 width, u32 height, OpenGLInfo& info) {
	GLuint handle;
	glGenTextures(1, &handle);
	if (handle != 1) {
		int breakhere = 2;
	}
	glBindTexture(GL_TEXTURE_2D, handle);
	glTexImage2D(GL_TEXTURE_2D, 0, info.defaultInternalTextureFormat,
		width, height, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glBindTexture(GL_TEXTURE_2D, 0);
	glFlush();
	return handle;
}

inline 
void OpenGLFreeTexture(GLuint handle) {
	glDeleteTextures(1, &handle);
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
			glBindTexture(GL_TEXTURE_2D, bitmap->textureHandle);
			OpenGLRenderRectangle(origin, origin + call->size, call->color);
		} break;
		InvalidDefaultCase;
		}
	}
}