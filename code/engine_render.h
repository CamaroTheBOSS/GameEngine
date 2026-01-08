#pragma once

/*
	1) All coordinates outside the renderer are expected to be bottom-up in Y and
	   left-right in X

	2) Bitmaps are expected to be bottom-up, ARGB with premultiplied alpha

	3) All distance/size metrics are expected to be in meters, not in pixels 
	   (unless it is explicitly specified)

	4) Colors should be in range <0, 1> RGBA, no premultiplied alpha, renderer
	   is responsible for correctly premultipling all the colors
*/

#define BITMAP_BYTES_PER_PIXEL 4
struct LoadedBitmap {
	void* bufferStart;
	u32* data;
	f32 heightOverWidth;
	i32 height;
	i32 width;
	i32 pitch;
	V2 align; // NOTE: bottom-up in pixels
};

struct EnvironmentMap {
	u32 mapWidth;
	u32 mapHeight;
	LoadedBitmap LOD[4];
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
	V4 color;
};

struct RenderCallRectangle {
	V3 center;
	V2 size;
	V2 offset;
	V4 color;
};

struct RenderCallBitmap {
	LoadedBitmap* bitmap;
	V3 center;
	V2 offset;
	V4 color;
};

struct RenderCallCoordinateSystem {
	V2 origin;
	V2 xAxis;
	V2 yAxis;
	V4 color;
	LoadedBitmap* bitmap;
	LoadedBitmap* normalMap;
	EnvironmentMap* topEnvMap;
	EnvironmentMap* middleEnvMap;
	EnvironmentMap* bottomEnvMap;
};

struct CameraProps {
	f32 focalLength;
	f32 distanceToTarget;
	f32 nearClip;
};

struct RenderGroup {
	CameraProps camera;
	f32 monitorWidth;
	f32 metersToPixels;

	u8* pushBuffer;
	u32 pushBufferSize;
	u32 maxPushBufferSize;
};

/*                Renderer API                  */
inline RenderCallClear* PushClearCall(RenderGroup& group, V4 color = V4{ 1, 1, 1, 1 });
inline RenderCallBitmap* PushBitmap(RenderGroup& group, LoadedBitmap* bitmap, V3 center, 
	V2 offset, V4 color = V4{1, 1, 1, 1});
inline RenderCallRectangle* PushRect(RenderGroup& group, V3 center, V2 size, 
	V2 offset, V4 color = V4{ 1, 1, 1, 1 });
/*                Renderer API                  */