#pragma once
#include "engine_common.h"
#include "engine_rand.h"

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

#define SOUND_CHUNK_SAMPLE_OVERLAP 8
struct LoadedSound {
	u32 sampleCount;
	u32 nChannels;
	f32* samples[2];
};

struct BitmapId {
	u32 id;
};

struct SoundId {
	u32 id;
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
	u64 assetMetadatasOffset;
	u64 assetsOffset;
};

struct AssetFileBitmapInfo {
	i32 height;
	i32 width;
	i32 pitch;
	V2 alignment;
	u32 dataSizeInBytes;
	u32 dataOffset;
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
	u32 dataSizeInBytes;
	u32 samplesOffset[2];
};

#pragma warning(push)
#pragma warning(disable : 4201)
struct AssetMetadata {
	union {
		AssetFileSoundInfo _soundInfo;
		AssetFileBitmapInfo _bitmapInfo;
	};
};

struct Asset {
	union {
		LoadedBitmap bitmap;
		LoadedSound sound;
	};
	u32 fileSourceIndex;
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
struct PlatformFileGroup;
struct PlatformFileHandle;
struct Assets {
	MemoryArena arena;
	TransientState* tranState;

	u32 assetCount;
	u32 assetMaxCount;
	Asset* assets;
	AssetMetadata* metadatas;
	AssetFeatures* features;
	AssetGroup groups[Asset_Count];

	PlatformFileGroup* sources;

	u32 totalMemoryMax;
	u32 totalMemoryUsed;
};

/* ------------------ Asset System API -------------------- */
internal bool PrefetchBitmap(Assets& assets, BitmapId bid);
internal bool PrefetchSound(Assets& assets, SoundId sid);
inline Asset* GetAsset(Assets& assets, u32 id);
inline AssetMetadata* GetAssetMetadata(Assets& assets, Asset& asset);
inline AssetFeatures* GetAssetFeatures(Assets& assets, u32 id);
inline PlatformFileHandle* GetAssetSource(Assets& assets, u32 index);
inline SoundId GetFirstSoundIdWithType(Assets& assets, AssetTypeID typeId);
inline SoundId GetRandomSoundId(Assets& assets, AssetTypeID typeId, RandomSeries& series);
inline SoundId GetBestFitSoundId(Assets& assets, AssetTypeID typeId, AssetFeatures match, AssetFeatures weight, f32 halfPeriod);
inline BitmapId GetFirstBitmapIdWithType(Assets& assets, AssetTypeID typeId);
inline BitmapId GetRandomBitmapId(Assets& assets, AssetTypeID typeId, RandomSeries& series);
inline BitmapId GetBestFitBitmapId(Assets& assets, AssetTypeID typeId, AssetFeatures match, AssetFeatures weight, f32 halfPeriod);
inline bool NeedsFetching(Asset& asset);
inline bool IsReady(Asset* asset);
inline bool IsValid(BitmapId bid);
inline bool IsValid(SoundId sid);