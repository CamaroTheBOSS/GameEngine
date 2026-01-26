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
	f32 widthOverHeight;
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

struct CameraProps {
	f32 focalLength;
	f32 distanceToTarget;
	f32 nearClip;
};

struct ProjectionProps {
	CameraProps camera;
	f32 monitorWidth;
	f32 metersToPixels;
	V2 screenCenter;
	bool orthographic;
};

struct RenderGroup {
	ProjectionProps projection;
	u8* pushBuffer;
	u32 pushBufferSize;
	u32 maxPushBufferSize;
};

enum AssetTypeID {
	Asset_Null,

	Asset_Tree,
	Asset_Player,
	Asset_Grass,
	Asset_Ground,

	Asset_Count
};

enum class AssetState {
	NotReady,
	Pending,
	Ready
};

struct Asset {
	LoadedBitmap bitmap;
	AssetState state;
};

struct AssetGroup {
	u32 firstAssetIndex;
	u32 lastAssetIndex;
};

struct BitmapId {
	u32 id;
};

struct SoundId {
	u32 id;
};

struct BitmapInfo {
	const char* filename;
	V2 alignment;
	AssetTypeID typeId;
};

struct TransientState;
struct Assets {
	MemoryArena arena;
	TransientState* tranState;

	u32 assetCount;
	BitmapInfo bitmapInfos[256 * Asset_Count];
	AssetGroup groups[Asset_Count];
	
	Asset assets[256 * Asset_Count];
	

	LoadedBitmap playerMoveAnim[4];
	LoadedBitmap groundBmps[2];
	LoadedBitmap grassBmps[2];
};

/*                Renderer API                  */
inline bool PushClearCall(RenderGroup& group, V4 color = V4{ 1, 1, 1, 1 });
inline bool PushBitmap(RenderGroup& group, LoadedBitmap* bitmap, V3 center, f32 height, 
	V2 offset, V4 color = V4{1, 1, 1, 1});
inline bool PushBitmap(RenderGroup& group, Assets& assets, AssetTypeID id, V3 center, 
	f32 height, V2 offset, V4 color);
inline bool PushRect(RenderGroup& group, V3 center, V2 size, V2 offset, 
	V4 color = V4{ 1, 1, 1, 1 });
/*                Renderer API                  */