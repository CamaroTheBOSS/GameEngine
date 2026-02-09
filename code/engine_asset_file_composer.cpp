#include "engine_render.cpp"
#include "engine_assets.cpp"

#include <stdlib.h>
#include <stdio.h>
#include <Windows.h>

inline
FileData ReadEntireFile(const char* filename) {
	if (!filename) {
		return {};
	}
	FILE* file = 0;
	errno_t err = fopen_s(&file, filename, "r");
	Assert(file);
	if (!file || err) {
		return {};
	}
	FileData data = {};
	fseek(file, 0, SEEK_END);
	data.size = ftell(file);
	data.content = VirtualAlloc(0, data.size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!data.content) {
		return {};
	}
	fseek(file, 0, SEEK_SET);
	fread(data.content, data.size, 1, file);
	fclose(file);
	return data;
}

internal
LoadedBitmap LoadBmpFile(const char* filename, V2 bottomUpAlignRatio = V2{ 0.5f, 0.5f })
{
#pragma pack(push, 1)
	struct BmpHeader {
		u16 signature; // must be 0x42 = BMP
		u32 fileSize;
		u32 reservedZeros;
		u32 bitmapOffset; // where pixels starts
		u32 headerSize;
		u32 width;
		u32 height;
		u16 planes;
		u16 bitsPerPixel;
		u32 compression;
		u32 imageSize;
		u32 resolutionPixPerMeterX;
		u32 resolutionPixPerMeterY;
		u32 colorsUsed;
		u32 ColorsImportant;
		u32 redMask;
		u32 greenMask;
		u32 blueMask;
		u32 alphaMask;
	};
#pragma pack(pop)

	FileData bmpData = ReadEntireFile(filename);
	if (!bmpData.content) {
		return {};
	}
	BmpHeader* header = ptrcast(BmpHeader, bmpData.content);
	Assert(header->compression == 3);
	Assert(header->bitsPerPixel == 32);
	u32 redShift = LeastSignificantHighBit(header->redMask).index;
	u32 greenShift = LeastSignificantHighBit(header->greenMask).index;
	u32 blueShift = LeastSignificantHighBit(header->blueMask).index;
	u32 alphaShift = LeastSignificantHighBit(header->alphaMask).index;

	LoadedBitmap result = {};
	result.bufferStart = ptrcast(void, ptrcast(u8, bmpData.content) + header->bitmapOffset);
	result.height = header->height;
	result.width = header->width;
	result.widthOverHeight = f4(result.width) / f4(result.height);
	Assert((header->bitsPerPixel / 8) == BITMAP_BYTES_PER_PIXEL);
	result.pitch = result.width * BITMAP_BYTES_PER_PIXEL;
	result.data = ptrcast(u32, result.bufferStart);
	result.align = bottomUpAlignRatio;

	u32* pixels = ptrcast(u32, result.bufferStart);
	for (u32 Y = 0; Y < header->height; Y++) {
		for (u32 X = 0; X < header->width; X++) {
			V4 texel = {
				f4((*pixels >> redShift) & 0xFF),
				f4((*pixels >> greenShift) & 0xFF),
				f4((*pixels >> blueShift) & 0xFF),
				f4((*pixels >> alphaShift) & 0xFF),
			};
			texel = SRGB255ToLinear1(texel);
			texel.RGB *= texel.A;
			texel = Linear1ToSRGB255(texel);
			*pixels++ = (u4(texel.A + 0.5f) << 24) +
				(u4(texel.R + 0.5f) << 16) +
				(u4(texel.G + 0.5f) << 8) +
				(u4(texel.B + 0.5f) << 0);
		}
	}

	return result;
}

internal
LoadedSound LoadWAV(const char* filename, u32 firstSampleIndex, u32 chunkSampleCount) {
#define CHUNK_ID(a, b, c, d) ((d << 24) + (c << 16) + (b << 8) + a)
#pragma pack(push, 1)
	struct RiffHeader {
		u32 riffId;
		u32 size;
		u32 waveId;
	};
	struct ChunkHeader {
		u32 chunkId;
		u32 chunkSize;
	};
	struct FmtHeader {
		u16 formatCode;
		u16 nChannels;
		u32 nSamplesPerSec;
		u32 nAvgBytesPerSec;
		u16 nBlockAlign;
		u16 bitsPerSample;
		u16 cbSize;
		u16 validBitsPerSample;
		u32 channelMask;
		char subformat[16];
	};
#pragma pack(pop)
	LoadedSound sound = {};
	FileData data = ReadEntireFile(filename);
	if (!data.content) {
		return sound;
	}

	RiffHeader* riffHeader = ptrcast(RiffHeader, data.content);
	Assert(riffHeader->riffId == CHUNK_ID('R', 'I', 'F', 'F'));
	Assert(riffHeader->waveId == CHUNK_ID('W', 'A', 'V', 'E'));
	u32 chunkSize = riffHeader->size - 4;
	u8* ptr = ptrcast(u8, data.content) + sizeof(RiffHeader);
	u32 sizeRead = 0;
	i16* fileSamples = 0;
	u32 samplesSizeInBytes = 0;
	u32 nChannels = 0;
	while (sizeRead < chunkSize) {
		ChunkHeader* header = ptrcast(ChunkHeader, ptr);
		sizeRead += sizeof(header);
		ptr += sizeof(header);
		switch (header->chunkId) {
		case CHUNK_ID('f', 'm', 't', ' '): {
			FmtHeader* fmt = ptrcast(FmtHeader, ptr);
			Assert(fmt->nSamplesPerSec == 48000);
			Assert(fmt->bitsPerSample == 16);
			Assert(fmt->nChannels <= 2);
			nChannels = fmt->nChannels;
		} break;
		case CHUNK_ID('d', 'a', 't', 'a'): {
			fileSamples = ptrcast(i16, ptr);
			samplesSizeInBytes = header->chunkSize;
		} break;
		}
		u32 paddedSize = (header->chunkSize + 1) & ~1;
		sizeRead += paddedSize;
		ptr += paddedSize;
	}
	if (chunkSampleCount == 0) {
		chunkSampleCount = U32_MAX;
	}
	u32 wavSampleCount = samplesSizeInBytes / (sizeof(i16) * nChannels);
	Assert(samplesSizeInBytes > 0);
	Assert(wavSampleCount > firstSampleIndex);
	Assert(nChannels == 2);
	sound.nChannels = nChannels;
	sound.sampleCount = Minimum(wavSampleCount - firstSampleIndex, chunkSampleCount);
	Assert(sound.sampleCount != 0);
	Assert((sound.sampleCount & 1) == 0);
	u64 bytesToAllocate = (sound.sampleCount + SOUND_CHUNK_SAMPLE_OVERLAP) * sizeof(f32) * sound.nChannels;
	sound.samples[0] = ptrcast(f32, VirtualAlloc(0, bytesToAllocate, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
	sound.samples[1] = sound.samples[0] + sound.sampleCount + SOUND_CHUNK_SAMPLE_OVERLAP;
	f32* dest[2] = { sound.samples[0], sound.samples[1] };
	i16* src = fileSamples + sound.nChannels * firstSampleIndex;
	f32 dividor = f4(I16_MAX);
	// NOTE: copy a little bit more samples if this is not last chank to make sound seamless
	bool lastChunk = (wavSampleCount - firstSampleIndex) <= chunkSampleCount;
	u32 samplesToCopy = sound.sampleCount;
	if (!lastChunk) {
		samplesToCopy += SOUND_CHUNK_SAMPLE_OVERLAP;
	}
	for (u32 sampleIndex = 0; sampleIndex < samplesToCopy; sampleIndex++) {
		for (u32 channelIndex = 0; channelIndex < sound.nChannels; channelIndex++) {
			*dest[channelIndex]++ = f4(*src++) / dividor;
		}
	}
	return sound;
}

inline
void AddBmpAsset(Assets& assets, AssetTypeID id, const char* filename, V2 alignment = V2{ 0.5f, 0.5f }) {
	Assert(assets.assetCount < assets.assetMaxCount);
	Asset* asset = &assets.assets[assets.assetCount];
	asset->bitmap = LoadBmpFile(filename, alignment);
	AssetFileBitmapInfo* info = &assets.metadatas[assets.assetCount]._bitmapInfo;
	info->alignment = alignment;
	info->height = asset->bitmap.height;
	info->width = asset->bitmap.width;
	info->pitch = asset->bitmap.pitch;
	asset->metadataId = assets.assetCount;
	AssetGroup* group = &assets.groups[id];
	if (group->firstAssetIndex == 0) {
		group->firstAssetIndex = assets.assetCount;
		group->onePastLastAssetIndex = group->firstAssetIndex + 1;
		group->type = AssetGroup_Bitmap;
	}
	else {
		Assert(group->type == AssetGroup_Bitmap);
		group->onePastLastAssetIndex++;
	}
	assets.assetCount++;
}

inline
SoundId AddSoundAsset(Assets& assets, AssetTypeID id, const char* filename, u32 firstSampleIndex = 0, u32 chunkSampleCount = 0) {
	Assert(assets.assetCount < assets.assetMaxCount);
	SoundId result = { assets.assetCount };
	Asset* asset = &assets.assets[assets.assetCount];
	asset->sound = LoadWAV(filename, firstSampleIndex, chunkSampleCount);
	AssetFileSoundInfo* info = &assets.metadatas[assets.assetCount]._soundInfo;
	info->chain = { SoundChain::None, 0 };
	info->nChannels = asset->sound.nChannels;
	info->sampleCount = asset->sound.sampleCount;
	asset->metadataId = assets.assetCount;
	AssetGroup* group = &assets.groups[id];
	if (group->firstAssetIndex == 0) {
		group->firstAssetIndex = assets.assetCount;
		group->onePastLastAssetIndex = group->firstAssetIndex + 1;
		group->type = AssetGroup_Sound;
	}
	else {
		Assert(group->type == AssetGroup_Sound);
		group->onePastLastAssetIndex++;
	}
	assets.assetCount++;
	return result;
}

internal
void AddFeature(Assets& assets, AssetFeatureID fId, f32 value) {
	Assert(assets.assetCount > 0);
	AssetFeatures* features = GetAssetFeatures(assets, assets.assetCount - 1);
	
	(*features)[fId] = value;
}

int main() {
	// NOTE: This is offline code, so doesn't need to be super cool
	void* memory = VirtualAlloc(0, MB(12), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	Assets assets = {};
	InitializeArena(assets.arena, memory, MB(12));
	assets.assetMaxCount = 256 * Asset_Count;
	assets.assets = PushArray(assets.arena, assets.assetMaxCount, Asset);
	assets.features = PushArray(assets.arena, assets.assetMaxCount, AssetFeatures);
	assets.metadatas = PushArray(assets.arena, assets.assetMaxCount, AssetMetadata);
	AddBmpAsset(assets, Asset_Null, 0);
	AddBmpAsset(assets, Asset_Tree, "test/tree.bmp", V2{ 0.5f, 0.25f });
	AddFeature(assets, Feature_Height, 1.f);
	AddBmpAsset(assets, Asset_Tree, "test/tree2.bmp", V2{ 0.5f, 0.25f });
	AddFeature(assets, Feature_Height, 3.f);
	AddBmpAsset(assets, Asset_Tree, "test/tree3.bmp", V2{ 0.5f, 0.25f });
	AddFeature(assets, Feature_Height, 2.f);
	AddBmpAsset(assets, Asset_Ground, "test/ground0.bmp");
	AddBmpAsset(assets, Asset_Ground, "test/ground1.bmp");
	AddBmpAsset(assets, Asset_Grass, "test/grass0.bmp");
	AddBmpAsset(assets, Asset_Grass, "test/grass1.bmp");

	V2 playerBitmapsAlignment = V2{ 0.5f, 0.2f };
	AddBmpAsset(assets, Asset_Player, "test/hero-right.bmp", playerBitmapsAlignment);
	AddFeature(assets, Feature_FacingDirection, 0.f * TAU);
	AddBmpAsset(assets, Asset_Player, "test/hero-up.bmp", playerBitmapsAlignment);
	AddFeature(assets, Feature_FacingDirection, 0.25f * TAU);
	AddBmpAsset(assets, Asset_Player, "test/hero-left.bmp", playerBitmapsAlignment);
	AddFeature(assets, Feature_FacingDirection, 0.5f * TAU);
	AddBmpAsset(assets, Asset_Player, "test/hero-down.bmp", playerBitmapsAlignment);
	AddFeature(assets, Feature_FacingDirection, 0.75f * TAU);

	u32 silksongSampleCount = 7762944;
	u32 chunkSampleCount = 4 * 48000;
	u32 firstSampleIndex = 0;
	Asset* prevAsset = 0;
	// TODO: What with feature based asset retrieval? It is possible for asset system to return
	// not first music chunk?
	while (firstSampleIndex < silksongSampleCount) {
		SoundId nextAssetId = AddSoundAsset(assets, Asset_Music, "sound/silksong.wav", firstSampleIndex, chunkSampleCount);
		Asset* nextAsset = GetAsset(assets, nextAssetId.id);
		if (prevAsset) {
			AssetMetadata* metadata = GetAssetMetadata(assets, *prevAsset);
			metadata->_soundInfo.chain = { SoundChain::Advance, 1 };
		}
		prevAsset = nextAsset;
		firstSampleIndex += chunkSampleCount;
	}
	AddSoundAsset(assets, Asset_Bloop, "sound/bloop2.wav");
	FILE* file;
	fopen_s(&file, "test.assf", "wb");
	if (!file) {
		printf("Cannot open file\n");
		exit(1);
	}

	AssetFileHeader header;
	header.assetsCount = assets.assetCount;
	header.featuresOffset = sizeof(header);
	header.assetGroupsOffset = header.featuresOffset + sizeof(AssetFeatures) * assets.assetCount;
	header.assetMetadatasOffset = header.assetGroupsOffset + sizeof(AssetGroup) * Asset_Count;
	header.assetsOffset = header.assetMetadatasOffset + sizeof(AssetMetadata) * assets.assetCount;
	fwrite(&header, sizeof(AssetFileHeader), 1, file);
	fwrite(assets.features, sizeof(AssetFeatures), assets.assetCount, file);
	fwrite(&assets.groups, sizeof(AssetGroup), Asset_Count, file);
	fseek(file, u4(header.assetsOffset), SEEK_SET);
	for (u32 assetGroupIndex = 0; assetGroupIndex < ArrayCount(assets.groups); assetGroupIndex++) {
		AssetGroup* group = assets.groups + assetGroupIndex;
		for (u32 assetIndex = group->firstAssetIndex; assetIndex < group->onePastLastAssetIndex; assetIndex++) {
			Asset* asset = GetAsset(assets, assetIndex);
			AssetMetadata* metadata = GetAssetMetadata(assets, *asset);
			if (group->type == AssetGroup_Bitmap) {
				u32 size = asset->bitmap.pitch * asset->bitmap.height;
				u32 dataPosition = ftell(file);
				fwrite(asset->bitmap.data, size, 1, file);
				metadata->_bitmapInfo.dataOffset = dataPosition;
			}
			else if (group->type == AssetGroup_Sound) {
				u32 samplesPosition0 = ftell(file);
				u32 count = asset->sound.sampleCount + SOUND_CHUNK_SAMPLE_OVERLAP;
				fwrite(asset->sound.samples[0], sizeof(f32), count, file);
				u32 samplesPosition1 = ftell(file);
				fwrite(asset->sound.samples[1], sizeof(f32), count, file);
				metadata->_soundInfo.samplesOffset[0] = samplesPosition0;
				metadata->_soundInfo.samplesOffset[1] = samplesPosition1;
			}
		}
	}
	fseek(file, u4(header.assetMetadatasOffset), SEEK_SET);
	fwrite(assets.metadatas, sizeof(AssetMetadata), assets.assetCount, file);
	fclose(file);
	return 0;
}