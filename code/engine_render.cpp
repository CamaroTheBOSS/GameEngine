#include "engine.h"
#include "engine_render.h"
#include "engine_intrinsics.h"
#include "engine_rand.h"

inline 
ObjectTransform DefaultUprightTransform() {
	ObjectTransform transform = {};
	transform.upright = true;
	return transform;
}

inline
ObjectTransform ScaledUprightTransform(f32 scale) {
	ObjectTransform transform = {};
	transform.scale = scale;
	transform.upright = true;
	return transform;
}

inline
ObjectTransform DefaultFlatTransform() {
	ObjectTransform transform = {};
	return transform;
}

inline
ObjectTransform ScaledFlatTransform(f32 scale) {
	ObjectTransform transform = {};
	transform.scale = scale;
	return transform;
}

internal
EntityBasis ProjectCoords(Projection& projection, ObjectTransform transform, V3 center, V2 size, f32 sortBias = 0.f)
{
	EntityBasis result = {};
	if (projection.orthographic) {
		result.center = projection.screenCenter + center.XY * projection.metersToPixels;
		result.size = size * projection.metersToPixels;
		result.valid = true;
	}
	else {
		V3 rawCoords = ToV3(center.XY, 1.f);
		f32 denominator = projection.camera.distanceToTarget - center.Z;
		if (denominator > projection.camera.nearClip) {
			V3 projectedCoords = projection.camera.focalLength * rawCoords / denominator;
			result.center = projection.screenCenter + projectedCoords.XY * projection.metersToPixels;
			result.size = projectedCoords.Z * size * projection.metersToPixels;
			result.valid = true;
		}
	}
	result.sortKey = 4096 * (center.Z + projection.offset.Z + 0.1f * transform.upright) - center.Y - projection.offset.Y + sortBias;
	return result;
}

inline
V2 FromPixelSpaceToWorldSpace(Projection& projection, V2 pixelSpacePos, f32 atDistanceFromCamera) {
	V2 worldSpace = (pixelSpacePos - projection.screenCenter) / projection.metersToPixels;
	if (!projection.orthographic) {
		f32 distanceFromTarget = projection.camera.distanceToTarget - atDistanceFromCamera;
		worldSpace = worldSpace * distanceFromTarget / projection.camera.focalLength;
	}
	return worldSpace;
}

internal 
V2 UnprojectSize(Projection& projection, V2 size) {
	return size / projection.metersToPixels;
}

inline
Rect2 GetRenderRectangleAtDistance(Projection& projection, u32 width, u32 height, f32 distance) {
	V2 screenDim = 0.5f * V2i(width, height) / projection.metersToPixels;
	V2 projectedDims = distance * screenDim / projection.camera.focalLength;
	Rect2 result = GetRectFromCenterHalfDim(V2{ 0, 0 }, projectedDims);
	return result;
}

inline
Rect2 GetRenderRectangleAtTarget(Projection& projection, u32 width, u32 height) {
	Rect2 result = GetRenderRectangleAtDistance(projection, width, height, projection.camera.distanceToTarget);
	return result;
}

inline
Camera GetStandardCamera() {
	Camera result = {};
	result.distanceToTarget = 10.f;
	result.focalLength = 0.7f;
	result.nearClip = 0.2f;
	return result;
}

inline
Projection GetOrtographicProjection(u32 widthPix, u32 heightPix, f32 metersToPixels) {
	Projection result = {};
	result.monitorWidth = 0.52f;
	result.metersToPixels = metersToPixels;
	result.screenCenter = { widthPix / 2.f,
							heightPix / 2.f };
	result.orthographic = true;
	result.camera = GetStandardCamera();
	return result;
}

inline
Projection GetStandardProjection(u32 widthPix, u32 heightPix) {
	Projection result = {};
	result.monitorWidth = 0.52f;
	result.metersToPixels = widthPix * result.monitorWidth;
	result.screenCenter = { widthPix / 2.f,
							heightPix / 2.f };
	result.orthographic = false;
	result.camera = GetStandardCamera();
	return result;
}

inline
RenderGroup BeginRendering(RenderCommandBuffer* commands, Assets* assets, bool renderInBackground) {
	RenderGroup result = {};
	result.commands = commands;
	result.renderInBackground = renderInBackground;
	result.generationId.id = 0;
	result.assets = assets;
	result.generationId = NewGenerationId(*assets);
	return result;
}
inline
void EndRendering(RenderGroup& group) {
	Assert(group.generationId.id);
	FinishGeneration(*group.assets, group.generationId);
	group.generationId.id = 0;
}

#define PushRenderEntry(group, type, sortKey) ptrcast(type, PushRenderEntry_(group, sizeof(type), RenderCallType_##type, sortKey))
inline
void* PushRenderEntry_(RenderGroup& group, u32 size, RenderCallType type, f32 sortKey) {
	RenderCommandBuffer* commands = group.commands;
	size += sizeof(RenderCallHeader);
	Assert(commands->pushBufferSize + size <= commands->sortBufferAt - sizeof(SortElement));
	if (commands->pushBufferSize + size > commands->sortBufferAt - sizeof(SortElement)) {
		return 0;
	}
	commands->sortBufferAt -= sizeof(SortElement);
	SortElement* sortElement = ptrcast(SortElement, commands->pushBuffer + commands->sortBufferAt);
	sortElement->key = sortKey;
	sortElement->offset = commands->pushBufferSize;

	RenderCallHeader* header = ptrcast(RenderCallHeader, commands->pushBuffer + commands->pushBufferSize);
	header->type = type;
	void* result = (header + 1);
	commands->pushBufferSize += size;
	commands->pushBufferCount++;
	return result;
}

inline
bool PushClearCall(RenderGroup& group, V4 color) {
	RenderCallClear* call = PushRenderEntry(group, RenderCallClear, -999999.f);
	call->color = color;
	return true;
}

inline
bool PushBitmap(RenderGroup& group, LoadedBitmap* bitmap, ObjectTransform transform, V3 center, V4 color, f32 sortBias) {
	V2 sizeUnprojected = transform.scale * V2{ bitmap->widthOverHeight, 1 };
	EntityBasis params = ProjectCoords(group.projection, transform, center, sizeUnprojected, sortBias);
	if (!params.valid) {
		return false;
	}
	RenderCallBitmap* call = PushRenderEntry(group, RenderCallBitmap, params.sortKey);
	call->bitmap = bitmap;
	call->center = params.center;
	call->offset = transform.offset;
	call->color = color;
	call->size = params.size;
	return true;
}

inline
bool PushBitmap(RenderGroup& group, ObjectTransform transform, BitmapId bid, V3 center, V4 color, f32 sortBias) {
	TIMED_FUNCTION;
	LoadedBitmap* bitmap = GetBitmap(*group.assets, bid, group.generationId);
	if (!bitmap && group.renderInBackground) {
		// Note: If rendering in background, we want to always grab the bitmap no matter
		// how long it takes to actually acquaire it
		PrefetchBitmap(*group.assets, bid, true);
		bitmap = GetBitmap(*group.assets, bid, group.generationId);
		if (!bitmap) {
#if 0
			Assert(!"This should not be allowed in development, but in case it will happen, "
				"we need to fallback gracefully");
#endif
			return false;
		}
	}
	if (bitmap) {
		PushBitmap(group, bitmap, transform, center, color, sortBias);
	}
	else {
		// NOTE: Background prefetching cannot be done from the background thread
		AssertMainThread;
		PrefetchBitmap(*group.assets, bid);
	}
	return true;
}

inline
bool PushRect(RenderGroup& group, ObjectTransform transform, V3 center, V2 size, V4 color, f32 sortBias) {
	EntityBasis params = ProjectCoords(group.projection, transform, center, size, sortBias);
	if (!params.valid) {
		return false;
	}
	RenderCallRectangle* call = PushRenderEntry(group, RenderCallRectangle, params.sortKey);
	call->center = params.center;
	call->size = params.size;
	call->offset = transform.offset;
	call->color = color;
	return true;
}

inline
bool PushRect(RenderGroup& group, ObjectTransform transform, Rect2 rectangle, f32 Z, V4 color, f32 sortBias) {
	return PushRect(group, transform, ToV3(GetCenter(rectangle), Z), GetDim(rectangle), color, sortBias);
}

inline
LoadedFont* GetOrPrefetchFont(RenderGroup& group, FontId fid) {
	LoadedFont* font = GetFont(*group.assets, fid, group.generationId);
	if (!font) {
		PrefetchFont(*group.assets, fid, group.renderInBackground);
		if (group.renderInBackground) {
			font = GetFont(*group.assets, fid, group.generationId);
		}
	}
	return font;
}

inline
bool PushRectBorders(RenderGroup& group, ObjectTransform transform, V3 center, V2 size, V4 color, f32 thickness, f32 sortBias) {
	V3 basePos = center;
	basePos.X = center.X - 0.5f * size.X;
	PushRect(group, transform, basePos, V2{ thickness, size.Y }, color, sortBias);
	basePos.X = center.X + 0.5f * size.X;
	PushRect(group, transform, basePos, V2{ thickness, size.Y }, color, sortBias);
	basePos = center;
	basePos.Y = center.Y - 0.5f * size.Y;
	PushRect(group, transform, basePos, V2{ size.X, thickness }, color, sortBias);
	basePos.Y = center.Y + 0.5f * size.Y;
	PushRect(group, transform, basePos, V2{ size.X, thickness }, color, sortBias);
	return true;
}

inline
bool PushRectOutlineInside(RenderGroup& group, ObjectTransform transform, Rect2 rect, f32 Z, V4 color, f32 thickness, f32 sortBias) {
	Rect2 bot = GetRectFromMinMax(rect.min, V2{ rect.max.X, rect.min.Y + thickness });
	Rect2 top = GetRectFromMinMax(V2{ rect.min.X, rect.max.Y - thickness }, rect.max);
	Rect2 left = GetRectFromMinMax(rect.min, V2{ rect.min.X + thickness, rect.max.Y });
	Rect2 right = GetRectFromMinMax(V2{ rect.max.X - thickness, rect.min.Y }, rect.max);
	PushRect(group, transform, bot, Z, color, sortBias);
	PushRect(group, transform, top, Z, color, sortBias);
	PushRect(group, transform, left, Z, color, sortBias);
	PushRect(group, transform, right, Z, color, sortBias);
	return true;
}

inline
bool PushCoordinateSystem(RenderGroup& group, V2 origin, V2 xAxis, V2 yAxis, V4 color,
	LoadedBitmap* bitmap, LoadedBitmap* normalMap, EnvironmentMap* topEnvMap,
	EnvironmentMap* middleEnvMap, EnvironmentMap* bottomEnvMap)
{
	RenderCallCoordinateSystem* call = PushRenderEntry(group, RenderCallCoordinateSystem, 0.f);
	call->origin = origin;
	call->xAxis = xAxis;
	call->yAxis = yAxis;
	call->color = color;
	call->bitmap = bitmap;
	call->normalMap = normalMap;
	call->topEnvMap = topEnvMap;
	call->middleEnvMap = middleEnvMap;
	call->bottomEnvMap = bottomEnvMap;
	return true;
}