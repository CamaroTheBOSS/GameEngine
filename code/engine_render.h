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

struct BitmapId {
	u32 id;
};

struct SoundId {
	u32 id;
};

struct LoadedSound {
	u32 sampleCount;
	u32 nChannels;
	f32* samples[2];
	SoundId nextChunkId;
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

enum AssetFeatureID {
	Feature_Height,
	Feature_FacingDirection,

	Feature_Count
};

enum AssetTypeID {
	Asset_Null,

	Asset_Tree,
	Asset_Player,
	Asset_Grass,
	Asset_Ground,

	Asset_Music,
	Asset_Bloop,

	Asset_Count
};

enum class AssetState {
	NotReady,
	Pending,
	Ready
};

using AssetFeatures = f32[Feature_Count];

#define EAF_MAGIC_STRING(a, b, c, d) ((d << 24) + (c << 16) + (b << 8) + a)
struct AssetFileHeader {
	u32 magicString = EAF_MAGIC_STRING('a', 's', 's', 'f');
	u32 version = 0;

	u32 assetsCount;
	u64 featuresOffset;
	u64 assetGroupsOffset;
	u64 assetInfosOffset;
	u64 assetsOffset;
};

struct AssetFileBitmapInfo {
	i32 height;
	i32 width;
	i32 pitch;
	V2 alignment;
	u32 dataOffset; //u32*
};
enum class SoundChain {
	None,
	Advance
};
struct SoundChainInfo {
	SoundChain op;
	u32 count;
};

struct AssetFileSoundInfo {
	u32 sampleCount;
	u32 nChannels;
	SoundChainInfo chain;
	u32 samplesOffset[2]; //f32*
};

struct AssetFileInfo {
	union {
		AssetFileBitmapInfo bmp;
		AssetFileSoundInfo sound;
	};
};

struct BitmapInfo {
	const char* filename;
	AssetTypeID typeId;
	V2 alignment;
};

struct SoundInfo {
	const char* filename;
	AssetTypeID typeId;
	SoundChainInfo chain;
	u32 firstSampleIndex;
	u32 chunkSampleCount;
};

#pragma warning(push)
#pragma warning(disable : 4201)
struct AssetMetadata {
	union {
		struct {
			SoundInfo soundInfo;
			AssetFileSoundInfo soundFileInfo;
		};
		struct {
			BitmapInfo bitmapInfo;
			AssetFileBitmapInfo bitmapFileInfo;
		};
	};
};

struct PlatformFileHandle;
struct Asset {
	union {
		LoadedBitmap bitmap;
		LoadedSound sound;
	};
	PlatformFileHandle* fileHandle;
	u32 metadataId;
	AssetState state;
};
#pragma warning(pop)
enum AssetGroupType {
	AssetGroup_Bitmap,
	AssetGroup_Sound
};

struct AssetGroup {
	u32 firstAssetIndex;
	u32 onePastLastAssetIndex;
	AssetGroupType type;
};

struct TransientState;
struct Assets {
	MemoryArena arena;
	TransientState* tranState;

	u32 assetCount;
	u32 assetMaxCount;
	Asset* assets;
	AssetMetadata* metadatas;
	AssetFeatures* features;
	AssetGroup groups[Asset_Count];
};

/*                Renderer API                  */
inline bool PushClearCall(RenderGroup& group, V4 color = V4{ 1, 1, 1, 1 });
inline bool PushBitmap(RenderGroup& group, LoadedBitmap* bitmap, V3 center, f32 height, 
	V2 offset, V4 color = V4{1, 1, 1, 1});
inline bool PushBitmap(RenderGroup& group, Assets& assets, BitmapId bid, V3 center,
	f32 height, V2 offset, V4 color = V4{ 1, 1, 1, 1 });
inline bool PushRect(RenderGroup& group, V3 center, V2 size, V2 offset, 
	V4 color = V4{ 1, 1, 1, 1 });
/*                Renderer API                  */