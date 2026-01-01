#pragma once

struct LoadedBitmap {
	void* bufferStart;
	u32* data;
	i32 height;
	i32 width;
	i32 pitch;
	u32 alignX;
	u32 alignY;
};

struct DrawCall {
	LoadedBitmap* bitmap;
	V3 center;
	V3 rectSize;
	f32 R, G, B, A;
	V2 offset;
};

struct RenderGroup {
	u32 count;
	DrawCall drawCalls[4096];
};
