#pragma once

constexpr f32 pixelsPerMeter = 42.85714f;
constexpr f32 metersPerPixel = 1.f / pixelsPerMeter;

#define BITMAP_BYTES_PER_PIXEL 4
struct LoadedBitmap {
	void* bufferStart;
	u32* data;
	i32 height;
	i32 width;
	i32 pitch;
	u32 alignX;
	u32 alignY;
};

enum class RenderCallType {
	RenderCallClear,
	RenderCallRectangle,
	RenderCallBitmap,
	RenderCallCoordinateSystem,
};

struct RenderCallHeader {
	RenderCallType type;
};

struct RenderCallClear {
	f32 R, G, B, A;
};

struct RenderCallRectangle {
	V3 center;
	V3 rectSize;
	f32 R, G, B, A;
	V2 offset;
};

struct RenderCallBitmap {
	LoadedBitmap* bitmap;
	V3 center;
	f32 alpha;
	V2 offset;
};

struct RenderCallCoordinateSystem {
	V2 origin;
	V2 xAxis;
	V2 yAxis;
	V4 color;
	LoadedBitmap* bitmap;
	LoadedBitmap* normalMap;
};

struct RenderGroup {
	u8* pushBuffer;
	u32 pushBufferSize;
	u32 maxPushBufferSize;
};
