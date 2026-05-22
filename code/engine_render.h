#pragma once
#include "engine_assets.h"

/*
	1) All coordinates outside the renderer are expected to be bottom-up in Y and
	   left-right in X

	2) Bitmaps are expected to be bottom-up, ARGB with premultiplied alpha

	3) All distance/size metrics are expected to be in meters, not in pixels 
	   (unless it is explicitly specified)

	4) Colors should be in range <0, 1> RGBA, no premultiplied alpha, renderer
	   is responsible for correctly premultipling all the colors
*/

struct EntityBasis {
	V2 center;
	V2 size;
	bool valid;
	f32 sortKey;
};

struct EnvironmentMap {
	u32 mapWidth;
	u32 mapHeight;
	LoadedBitmap LOD[4];
};

enum RenderCallType {
	RenderCallType_RenderCallClear,
	RenderCallType_RenderCallRectangle,
	RenderCallType_RenderCallBitmap,
	RenderCallType_RenderCallCoordinateSystem,
};

struct RenderCallHeader {
	RenderCallType type;
};

struct RenderCallClear {
	V4 color;
};

struct RenderCallRectangle {
	V2 center;
	V2 size;
	V2 offset;
	V4 color;
};

struct RenderCallBitmap {
	LoadedBitmap* bitmap;
	V2 center;
	V2 offset;
	V2 size;
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

struct Camera {
	f32 focalLength;
	f32 distanceToTarget;
	f32 nearClip;
};

struct Projection {
	Camera camera;
	f32 monitorWidth;
	f32 metersToPixels;
	V2 screenCenter;
	bool orthographic;
	V3 offset;
};

struct ObjectTransform {
	bool upright;
	f32 scale;
	V2 offset;
};

struct RenderCommandBuffer;
struct RenderGroup {
	RenderCommandBuffer* commands;
	Projection projection;
	bool renderInBackground;
	GenerationId generationId;
	Assets* assets;
};

/*                Renderer API                  */
inline bool PushClearCall(RenderGroup& group, V4 color = V4{ 1, 1, 1, 1 });
inline bool PushBitmap(RenderGroup& group, LoadedBitmap* bitmap, ObjectTransform transform, V3 center,
	V4 color = V4{ 1, 1, 1, 1 }, f32 sortBias = 0.f);
inline bool PushBitmap(RenderGroup& group, ObjectTransform transform, BitmapId bid, V3 center,
	V4 color = V4{ 1, 1, 1, 1 }, f32 sortBias = 0.f);
inline bool PushRect(RenderGroup& group, ObjectTransform transform, V3 center, V2 size,
	V4 color = V4{ 1, 1, 1, 1 }, f32 sortBias = 0.f);
inline bool PushRect(RenderGroup& group, ObjectTransform transform, Rect2 rectangle, f32 Z, V4 color, f32 sortBias = 0.f);
inline bool PushRectBorders(RenderGroup& group, ObjectTransform transform, V3 center, V2 size, V4 color, f32 thickness, f32 sortBias = 0.f);
inline bool PushRectOutlineInside(RenderGroup& group, ObjectTransform transform, Rect2 rect, f32 Z, V4 color, f32 thickness, f32 sortBias = 0.f);

struct RenderCommandBuffer;
inline RenderGroup BeginRendering(RenderCommandBuffer* commands, Assets* assets, bool renderInBackground = false);
inline void EndRendering(RenderGroup& group);

inline V2 FromPixelSpaceToWorldSpace(Projection& projection, V2 pixelSpacePos, f32 atDistanceFromCamera);
inline Rect2 GetRenderRectangleAtDistance(Projection& projection, u32 width, u32 height, f32 distance);
inline Projection GetOrtographicProjection(u32 widthPix, u32 heightPix, f32 metersToPixels);
inline LoadedFont* GetOrPrefetchFont(RenderGroup& group, FontId fid);
inline FontId GetFontWithType(Assets& assets, FontType type);

inline ObjectTransform DefaultUprightTransform();
inline ObjectTransform ScaledUprightTransform(f32 scale);
inline ObjectTransform DefaultFlatTransform();
inline ObjectTransform ScaledFlatTransform(f32 scale);
/*                Renderer API                  */