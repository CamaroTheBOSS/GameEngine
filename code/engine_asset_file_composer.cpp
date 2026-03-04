#include "engine_render.cpp"
#include "engine_assets.cpp"

#include <stdlib.h>
#include <stdio.h>
#include <Windows.h>

u32 ASSET_MAX_COUNT = 256 * Asset_Count;
u32 MAX_UNICODE_CODEPOINT = 0x10FFFF;
bool duringFontAdding = false;

void RenderFilledRectangleOptimized(LoadedBitmap& bitmap, V2 origin, V2 xAxis, V2 yAxis, V4 color,
	bool even, Rect2i clipRect) {}
void RenderRectangleOptimized(LoadedBitmap& bitmap, V2 origin, V2 xAxis, V2 yAxis, V4 color,
	LoadedBitmap& texture, bool even, Rect2i clipRect) {}
DebugMemory* debugGlobalMemory;
u32 debugRecordsCount_Main;
u32 debugRecordsCount_Optimized;
DebugGlobalState* debugGlobalState;

struct Win32FontInfo {
	HFONT font;
	HDC dc;
	HBITMAP bmp;
	BITMAPINFO info;
	void* bits;

	u32 tmHeight;
	u32 tmAscent;
	u32 tmDescent;
	u32 tmInternalLeading;
	u32 tmExternalLeading;
};

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
Asset* AddAsset(Assets& assets, AssetTypeID id, AssetGroupType groupType) {
	Assert(assets.assetCount < ASSET_MAX_COUNT);
	Assert(!duringFontAdding || id == Asset_Font || id == Asset_FontGlyph);
	Asset* asset = &assets.assets[assets.assetCount];
	asset->memory = ptrcast(AssetMemoryHeader, malloc(sizeof(AssetMemoryHeader)));
	asset->metadataId = assets.assetCount;
	AssetGroup* group = &assets.groups[id];
	if (group->firstAssetIndex == 0) {
		group->firstAssetIndex = assets.assetCount;
		group->onePastLastAssetIndex = group->firstAssetIndex + 1;
		group->type = groupType;
	}
	else {
		Assert(group->type == groupType);
		group->onePastLastAssetIndex++;
	}
	assets.assetCount++;
	return asset;
}

inline
void AddBmpAsset(Assets& assets, AssetTypeID id, const char* filename, V2 alignment = V2{ 0.5f, 0.5f }) {
	Asset* asset = AddAsset(assets, id, AssetGroup_Bitmap);
	asset->memory->bitmap = LoadBmpFile(filename, alignment);
	AssetFileBitmapInfo* info = &assets.metadatas[asset->metadataId]._bitmapInfo;
	info->alignment = alignment;
	info->height = asset->memory->bitmap.height;
	info->width = asset->memory->bitmap.width;
	info->pitch = asset->memory->bitmap.pitch;
}

inline
SoundId AddSoundAsset(Assets& assets, AssetTypeID id, const char* filename, u32 firstSampleIndex = 0, u32 chunkSampleCount = 0) {
	Asset* asset = AddAsset(assets, id, AssetGroup_Sound);
	asset->memory->sound = LoadWAV(filename, firstSampleIndex, chunkSampleCount);
	AssetFileSoundInfo* info = &assets.metadatas[asset->metadataId]._soundInfo;
	info->chain = { SoundChain::None, 0 };
	info->nChannels = asset->memory->sound.nChannels;
	info->sampleCount = asset->memory->sound.sampleCount;
	return { asset->metadataId };
}

internal
Win32FontInfo CreateAssetFont(const char* fontName) {
	i32 srcHeight = 128;
	i32 srcWidth = 128;
	u32 srcPitch = srcWidth * BITMAP_BYTES_PER_PIXEL;

	AddFontResourceExA(fontName, FR_PRIVATE, 0);
	HFONT font = CreateFontA(
		srcHeight, // Height
		0, 0, 0,
		FW_DONTCARE, // Boldness
		false, // Italic
		false, // Underline
		false, // Strike-out
		DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS,
		ANTIALIASED_QUALITY,
		DEFAULT_PITCH | FF_DONTCARE,
		fontName
	);

	BITMAPINFO bmpinfo = {};
	bmpinfo.bmiHeader.biSize = sizeof(BITMAPINFO);
	bmpinfo.bmiHeader.biWidth = srcWidth + 16;
	bmpinfo.bmiHeader.biHeight = srcHeight + 16;
	bmpinfo.bmiHeader.biPlanes = 1;
	bmpinfo.bmiHeader.biBitCount = 32;
	bmpinfo.bmiHeader.biCompression = BI_RGB;
	bmpinfo.bmiHeader.biSizeImage = 0;
	bmpinfo.bmiHeader.biXPelsPerMeter = 0;
	bmpinfo.bmiHeader.biYPelsPerMeter = 0;
	bmpinfo.bmiHeader.biClrUsed = 0;
	bmpinfo.bmiHeader.biClrImportant = 0;
	void* bits = 0;
	HDC dc = CreateCompatibleDC(GetDC(0));
	HBITMAP bmp = CreateDIBSection(dc, &bmpinfo, DIB_RGB_COLORS, &bits, 0, 0);
	SelectObject(dc, bmp);
	SelectObject(dc, font);
	SetBkColor(dc, RGB(0, 0, 0));
	SetTextColor(dc, RGB(255, 255, 255));

	TEXTMETRICA metrics;
	GetTextMetricsA(dc, &metrics);
	Win32FontInfo result = {};
	result.dc = dc;
	result.bmp = bmp;
	result.font = font;
	result.bits = bits;
	result.info = bmpinfo;
	result.tmHeight = metrics.tmHeight;
	result.tmAscent = metrics.tmAscent;
	result.tmDescent = metrics.tmDescent;
	result.tmInternalLeading = metrics.tmInternalLeading;
	result.tmExternalLeading = metrics.tmExternalLeading;
	return result;
}

internal
void AddGlyphAsset(Assets& assets, LoadedFont& font, Win32FontInfo& fontInfo, u32 codepoint) {
	u32 srcHeight = fontInfo.info.bmiHeader.biHeight;
	u32 srcWidth = fontInfo.info.bmiHeader.biWidth;
	u32 srcPitch = srcWidth * BITMAP_BYTES_PER_PIXEL;
	u32 heightAdvance = srcHeight - fontInfo.tmHeight;
	void* bits = fontInfo.bits;
	memset(bits, 0, srcHeight * srcPitch);

	u32 widthInitAdvance = 8;
	BOOL result = TextOutW(fontInfo.dc, widthInitAdvance, 0, ptrcast(wchar_t, &codepoint), 1);
	ABC* abcForChar = ptrcast(ABC, malloc(sizeof(ABC)));
	GetCharABCWidthsW(fontInfo.dc, codepoint, codepoint, abcForChar);

	u32 minX = U32_MAX;
	u32 maxX = 0;
	u32 minY = U32_MAX;
	u32 maxY = 0;
	u32* srcPixel = ptrcast(u32, bits);
	for (u32 Y = 0; Y < u4(srcHeight); Y++) {
		bool foundNonZeroOnRow = false;
		for (u32 X = 0; X < u4(srcWidth); X++) {
			u32 color = *srcPixel++;
			if (color == 0) {
				continue;
			}
			if (Y < minY) {
				minY = Y;
			}
			if (Y >= maxY) {
				maxY = Y + 1;
			}
			if (X < minX) {
				minX = X;
			}
			if (X >= maxX) {
				maxX = X + 1;
			}
			
		}	
	}
	Asset* asset = AddAsset(assets, Asset_FontGlyph, AssetGroup_Bitmap);
	if (codepoint != ' ') {
		Assert(minY > 0 && minX > 0);
		Assert(minY < maxY && minX < maxX);
		Assert(maxY < u4(srcHeight - 2) && maxX < u4(srcWidth - 2));
		minX--;
		minY--;
		maxX++;
		maxY++;
		u32 dstWidth = maxX - minX;
		u32 dstHeight = maxY - minY;

		u32 dstPitch = dstWidth * BITMAP_BYTES_PER_PIXEL;
		u32 allocSize = dstHeight * dstPitch;
		u32* data = ptrcast(u32, malloc(allocSize));
		memset(data, 0, allocSize);
		u32* dstRow = data;
		u8* srcRow = ptrcast(u8, bits) + minY * srcPitch + minX * BITMAP_BYTES_PER_PIXEL;
		for (u32 Y = minY; Y < maxY; Y++) {
			srcPixel = ptrcast(u32, srcRow);
			for (u32 X = minX; X < maxX; X++) {
				u8 alpha = *(ptrcast(u8, srcPixel));
				V4 texel = {
					f4(255), f4(255), f4(255), f4(alpha),
				};
				texel = SRGB255ToLinear1(texel);
				texel.RGB *= texel.A;
				texel = Linear1ToSRGB255(texel);
				*dstRow++ = (u4(texel.A + 0.5f) << 24) +
					(u4(texel.R + 0.5f) << 16) +
					(u4(texel.G + 0.5f) << 8) +
					(u4(texel.B + 0.5f) << 0);
				srcPixel++;
			}
			srcRow += srcPitch;
		}
		
		f32 yAlignment = (f4(fontInfo.tmDescent + heightAdvance) - f4(minY)) / f4(dstHeight);
		f32 xAlignment = f4(1 - abcForChar->abcA) / f4(dstWidth);
		asset->memory->bitmap.data = data;
		asset->memory->bitmap.align = V2{ xAlignment, yAlignment };
		asset->memory->bitmap.height = dstHeight;
		asset->memory->bitmap.width = dstWidth;
		asset->memory->bitmap.pitch = dstPitch;
	}
	else {
		asset->memory->bitmap = {};
	}

	AssetFileBitmapInfo* info = &assets.metadatas[asset->metadataId]._bitmapInfo;
	info->alignment = asset->memory->bitmap.align;
	info->height = asset->memory->bitmap.height;
	info->width = asset->memory->bitmap.width;
	info->pitch = asset->memory->bitmap.pitch;
	if (font.onePastMaxCodepoint <= codepoint) {
		font.onePastMaxCodepoint = codepoint + 1;
	}
	Assert(font.onePastMaxLogicalIndex < U16_MAX);
	font.codepointToLogicalIndex[codepoint] = font.onePastMaxLogicalIndex++;
	free(abcForChar);
}

internal
void AddFontAsset(Assets& assets, LoadedFont& font, Win32FontInfo& fontInfo) {
	Assert(font.onePastMaxCodepoint > 0);

	u32 kerningTableElements = font.onePastMaxLogicalIndex * font.onePastMaxLogicalIndex;
	u32 kerningTableSize = kerningTableElements * sizeof(font.kerningTable[0]);
	font.kerningTable = ptrcast(u8, malloc(kerningTableSize));
	memset(font.kerningTable, 0, kerningTableSize);

	DWORD nPairs = GetKerningPairsW(fontInfo.dc, 0, 0);
	KERNINGPAIR* kPairs = ptrcast(KERNINGPAIR, malloc(nPairs * sizeof(KERNINGPAIR)));
	GetKerningPairsW(fontInfo.dc, nPairs, kPairs);

	ABC* abcStructs = ptrcast(ABC, malloc((font.onePastMaxCodepoint) * sizeof(ABC)));
	GetCharABCWidthsW(fontInfo.dc, 0, font.onePastMaxCodepoint - 1, abcStructs);
#if 0
	for (u32 firstCodepoint = 0; firstCodepoint < font.onePastMaxCodepoint; firstCodepoint++) {
		ABC* abc = abcStructs + firstCodepoint;
		for (u32 secondCodepoint = 0; secondCodepoint < font.onePastMaxCodepoint; secondCodepoint++) {
			u32 index = firstCodepoint * font.onePastMaxCodepoint + secondCodepoint;
			u32 kerningValue = abc->abcA + abc->abcB + abc->abcC;
			Assert(kerningValue <= 255);
			font.kerningTable[index] = (firstCodepoint != 0) ? 
				scast(u8, kerningValue) :
				0;
		}
	}
#else
	for (u32 firstCodepoint = 0; firstCodepoint < font.onePastMaxCodepoint; firstCodepoint++) {
		u32 firstKerningIndex = font.codepointToLogicalIndex[firstCodepoint];
		if (firstKerningIndex == 0) {
			// NOTE: Codepoint was not added to font, skip it;
			continue;
		}
		ABC* abc = abcStructs + firstCodepoint;
		for (u32 secondCodepoint = 0; secondCodepoint < font.onePastMaxCodepoint; secondCodepoint++) {
			u32 secondKerningIndex = font.codepointToLogicalIndex[secondCodepoint];
			if (secondKerningIndex == 0) {
				// NOTE: Codepoint was not added to font, skip it;
				continue;
			}
			Assert(firstKerningIndex < font.onePastMaxLogicalIndex);
			Assert(secondKerningIndex < font.onePastMaxLogicalIndex);
			u32 index = firstKerningIndex * font.onePastMaxLogicalIndex + secondKerningIndex;
			u32 kerningValue = abc->abcA + abc->abcB + abc->abcC;
			Assert(kerningValue <= 255);
			font.kerningTable[index] = (firstCodepoint != 0) ?
				scast(u8, kerningValue) :
				0;
		}
	}
#endif

	for (u32 kIndex = 0; kIndex < nPairs; kIndex++) {
		KERNINGPAIR* pair = kPairs + kIndex;
		if (pair->wFirst >= font.onePastMaxCodepoint ||
			pair->wSecond >= font.onePastMaxCodepoint ||
			pair->wFirst == 0) {
			continue;
		}
		u32 firstKerningIndex = font.codepointToLogicalIndex[pair->wFirst];
		u32 secondKerningIndex = font.codepointToLogicalIndex[pair->wSecond];
		if (firstKerningIndex == 0 || secondKerningIndex == 0) {
			// NOTE: Codepoint was not added to font, skip it;
			continue;
		}
		Assert(firstKerningIndex < font.onePastMaxLogicalIndex);
		Assert(secondKerningIndex < font.onePastMaxLogicalIndex);
		u32 index = firstKerningIndex * font.onePastMaxLogicalIndex + secondKerningIndex;
		Assert(index < kerningTableElements);
		u32 newKerningValue = font.kerningTable[index] + pair->iKernAmount;
		Assert(newKerningValue <= 255);
		font.kerningTable[index] = scast(u8, newKerningValue);
	}
	free(abcStructs);
	free(kPairs);

	Asset* asset = AddAsset(assets, Asset_Font, AssetGroup_Font);
	asset->memory->font.kerningTable = font.kerningTable;
	asset->memory->font.codepointToLogicalIndex = font.codepointToLogicalIndex;
	AssetFileFontInfo* info = &assets.metadatas[asset->metadataId]._fontInfo;
	info->onePastMaxCodepoint = font.onePastMaxCodepoint;
	info->onePastMaxLogicalIndex = font.onePastMaxLogicalIndex;
	info->logicalIndexBaseForGlyphs = font.logicalIndexBaseForGlyphs;
	info->metrics.ascent = fontInfo.tmAscent;
	info->metrics.descent = fontInfo.tmDescent;
	info->metrics.internalLeading = fontInfo.tmInternalLeading;
	info->metrics.externalLeading = fontInfo.tmExternalLeading;
}

inline
void FreeAssetFileFont(Win32FontInfo& fontInfo) {
	DeleteObject(fontInfo.bmp);
	DeleteObject(fontInfo.font);
	DeleteObject(fontInfo.dc);
}

internal
void AddFeature(Assets& assets, AssetFeatureID fId, f32 value) {
	Assert(assets.assetCount > 0);
	AssetFeatures* features = GetAssetFeatures(assets, assets.assetCount - 1);
	
	(*features)[fId] = value;
}

Assets InitializeAssets() {
	// NOTE: This is offline code, so doesn't need to be super cool
	Assets assets = {};
	assets.assets = ptrcast(Asset, malloc(ASSET_MAX_COUNT * sizeof(Asset)));
	assets.features = ptrcast(AssetFeatures, malloc(ASSET_MAX_COUNT * sizeof(AssetFeatures)));
	assets.metadatas = ptrcast(AssetMetadata, malloc(ASSET_MAX_COUNT * sizeof(AssetMetadata)));
	AddBmpAsset(assets, Asset_Null, 0);
	return assets;
}

void WriteAssetsToFile(Assets& assets, const char* filename) {
	FILE* file;
	fopen_s(&file, filename, "wb");
	if (!file) {
		printf("Cannot open file\n");
		return;
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
				u32 size = asset->memory->bitmap.pitch * asset->memory->bitmap.height;
				u32 dataPosition = ftell(file);
				fwrite(asset->memory->bitmap.data, size, 1, file);
				metadata->_bitmapInfo.dataOffset = dataPosition;
			}
			else if (group->type == AssetGroup_Sound) {
				u32 samplesPosition0 = ftell(file);
				u32 count = asset->memory->sound.sampleCount + SOUND_CHUNK_SAMPLE_OVERLAP;
				fwrite(asset->memory->sound.samples[0], sizeof(f32), count, file);
				u32 samplesPosition1 = ftell(file);
				fwrite(asset->memory->sound.samples[1], sizeof(f32), count, file);
				metadata->_soundInfo.samplesOffset[0] = samplesPosition0;
				metadata->_soundInfo.samplesOffset[1] = samplesPosition1;
			}
			else if (group->type == AssetGroup_Font) {
				u32 codePointCount = metadata->_fontInfo.onePastMaxCodepoint;
				u32 dataPosition = ftell(file);
				fwrite(asset->memory->font.codepointToLogicalIndex, sizeof(u16), codePointCount, file);

				u32 kerningCount = metadata->_fontInfo.onePastMaxLogicalIndex * metadata->_fontInfo.onePastMaxLogicalIndex;
				//u32 dataPosition = ftell(file);
				fwrite(asset->memory->font.kerningTable, sizeof(asset->memory->font.kerningTable[0]), kerningCount, file);
				metadata->_fontInfo.dataOffset = dataPosition;
			}
		}
	}
	fseek(file, u4(header.assetMetadatasOffset), SEEK_SET);
	fwrite(assets.metadatas, sizeof(AssetMetadata), assets.assetCount, file);
	fclose(file);
}

void WriteSounds() {
	Assets assets = InitializeAssets();
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
	WriteAssetsToFile(assets, "sounds.assf");
}

void WriteBitmaps() {
	Assets assets = InitializeAssets();
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

	WriteAssetsToFile(assets, "bitmaps.assf");
}

struct AddedFontHandle {
	Win32FontInfo win32Font;
	LoadedFont assetFont;
};

LoadedFont BeginFontAdding(Assets& assets) {
	duringFontAdding = true;
	LoadedFont assetFont = {};
	u32 allocSize = MAX_UNICODE_CODEPOINT * sizeof(u16);
	AssetGroup* group = &assets.groups[Asset_FontGlyph];
	assetFont.logicalIndexBaseForGlyphs = group->onePastLastAssetIndex - group->firstAssetIndex;
	assetFont.onePastMaxLogicalIndex = 1; // NOTE: Leave NULL kerning index alone
	assetFont.codepointToLogicalIndex = ptrcast(u16, malloc(allocSize));
	memset(assetFont.codepointToLogicalIndex, 0, allocSize);
	return assetFont;
}

void EndFontAdding(Assets& assets, AddedFontHandle& fontHandle) {
	duringFontAdding = false;
	AddFontAsset(assets, fontHandle.assetFont, fontHandle.win32Font);
	FreeAssetFileFont(fontHandle.win32Font);
}



AddedFontHandle BeginFontAddingWithStandardGlyphs(Assets& assets, const char* fontname) {
	AddedFontHandle result = {};
	result.win32Font = CreateAssetFont(fontname);
	result.assetFont = BeginFontAdding(assets);
	for (char c = ' '; c <= '~'; c++) {
		AddGlyphAsset(assets, result.assetFont, result.win32Font, c);
	}
	// ¿ó³æ
#if 1
	AddGlyphAsset(assets, result.assetFont, result.win32Font, 0x017C);
	AddGlyphAsset(assets, result.assetFont, result.win32Font, 0x00F3);
	AddGlyphAsset(assets, result.assetFont, result.win32Font, 0x0142);
	AddGlyphAsset(assets, result.assetFont, result.win32Font, 0x0107);
#endif
	return result;
}

void WriteFonts() {
	// NOTE: All assets within a group must be added next to each other in memory,
	// so when adding multiple fonts, we need firstly to add all the glyphs for all the fonts we
	// plan to add and after that, all the font handlers
	Assets assets = InitializeAssets();
	AddedFontHandle arial = BeginFontAddingWithStandardGlyphs(assets, "Arial");
	AddedFontHandle cascadia = BeginFontAddingWithStandardGlyphs(assets, "Cascadia Mono");
	// NOTE: Add in specific order, look for order in engine_assets.h -> enum FontType
	EndFontAdding(assets, cascadia);
	EndFontAdding(assets, arial);
	
	
	WriteAssetsToFile(assets, "fonts.assf");
}

int main() {
	WriteSounds();
	WriteBitmaps();
	WriteFonts();
	return 0;
}