#include "engine.h"

inline
void AddMemoryHeaderToList(Assets& assets, AssetMemoryHeader* header) {
	header->next = assets.lruSentinel.next;
	header->prev = &assets.lruSentinel;
	header->next->prev = header;
	header->prev->next = header;
}

inline
void RemoveMemoryHeaderFromList(AssetMemoryHeader* header) {
	header->next->prev = header->prev;
	header->prev->next = header->next;
}

inline
void MoveMemoryHeaderToFront(Assets& assets, AssetMemoryHeader* header) {
	RemoveMemoryHeaderFromList(header);
	AddMemoryHeaderToList(assets, header);
}

inline
bool IsValid(SoundId sid) {
	return sid.id > 0;
}

inline
bool IsValid(BitmapId bid) {
	return bid.id > 0;
}

inline
Asset* GetAsset(Assets& assets, u32 id) {
	Asset* asset = &assets.assets[id];
	return asset;
}

inline
LoadedBitmap* GetBitmap(Assets& assets, BitmapId bid) {
	Asset* asset = GetAsset(assets, bid.id);
	if (!asset || !asset->memory) {
		return 0;
	}
	Assert(asset->memory->type == AssetData_Bitmap);
	MoveMemoryHeaderToFront(assets, asset->memory);
	LoadedBitmap* bitmap = &asset->memory->bitmap;
	return bitmap;
}

inline
LoadedSound* GetSound(Assets& assets, SoundId sid) {
	Asset* asset = GetAsset(assets, sid.id);
	if (!asset || !asset->memory) {
		return 0;
	}
	Assert(asset->memory->type == AssetData_Sound);
	MoveMemoryHeaderToFront(assets, asset->memory);
	LoadedSound* sound = &asset->memory->sound;
	return sound;
}

inline
AssetMetadata* GetAssetMetadata(Assets& assets, Asset& asset) {
	AssetMetadata* metadata = &assets.metadatas[asset.metadataId];
	return metadata;
}

inline
AssetFeatures* GetAssetFeatures(Assets& assets, u32 id) {
	AssetFeatures* asset = &assets.features[id];
	return asset;
}

inline
PlatformFileHandle* GetAssetSource(Assets& assets, u32 index) {
	Assert(index < assets.sources->count);
	PlatformFileHandle* handle = *(assets.sources->files + index);
	return handle;
}

inline
bool IsReady(Asset* asset) {
	return asset && asset->state == AssetState::Ready;
}

inline
AssetGroup* GetAssetGroup(Assets& assets, AssetTypeID typeId) {
	AssetGroup* group = &assets.groups[typeId];
	Assert(group);
	// TESTING NOTE: These are handy in development, comment it out when want to test
	// whether asset system is resistent on lack of assets loaded
#if 1
	Assert(group->firstAssetIndex != 0);
	Assert(group->firstAssetIndex < group->onePastLastAssetIndex);
#endif
	return group;
}

inline
u32 _GetFirstAssetIdWithType(Assets& assets, AssetTypeID typeId) {
	AssetGroup* group = GetAssetGroup(assets, typeId);
	return group->firstAssetIndex;
}

inline
u32 _GetRandomAssetId(Assets& assets, AssetTypeID typeId, RandomSeries& series) {
	AssetGroup* group = GetAssetGroup(assets, typeId);
	u32 id = RandomChoiceBetween(series, group->firstAssetIndex, group->onePastLastAssetIndex);
	return id;
}

internal
u32 _GetBestFitAssetId(Assets& assets, AssetTypeID typeId, AssetFeatures match, AssetFeatures weight, f32 halfPeriod) {
	AssetGroup* group = GetAssetGroup(assets, typeId);
	u32 best = {};
	f32 bestScore = F32_MAX;
	for (u32 assetIndex = group->firstAssetIndex;
		assetIndex < group->onePastLastAssetIndex;
		assetIndex++
		) {
		Asset* asset = GetAsset(assets, assetIndex);
		if (!asset) {
			continue;
		}
		AssetFeatures* features = GetAssetFeatures(assets, assetIndex);
		f32 score = 0;
		for (u32 featureIndex = 0; featureIndex < ArrayCount(*features); featureIndex++) {
			//f32 sign = SignF32(halfPeriod - match[featureIndex]);
			f32 a1 = (*features)[featureIndex];
			f32 a2 = (*features)[featureIndex] - 2 * halfPeriod;
			f32 d1 = Abs(match[featureIndex] - a1);
			f32 d2 = Abs(match[featureIndex] - a2);
			f32 distance = Minimum(d1, d2);
			score += weight[featureIndex] * Abs(distance);
		}
		if (score < bestScore) {
			bestScore = score;
			best = assetIndex;
		}
	}
	return best;
}

inline
SoundId GetFirstSoundIdWithType(Assets& assets, AssetTypeID typeId) {
	SoundId id = { _GetFirstAssetIdWithType(assets, typeId) };
	return id;
}

inline
SoundId GetRandomSoundId(Assets& assets, AssetTypeID typeId, RandomSeries& series) {
	SoundId id = { _GetRandomAssetId(assets, typeId, series) };
	return id;
}

inline
SoundId GetBestFitSoundId(Assets& assets, AssetTypeID typeId, AssetFeatures match, AssetFeatures weight, f32 halfPeriod) {
	SoundId id = { _GetBestFitAssetId(assets, typeId, match, weight, halfPeriod) };
	return id;
}

inline
BitmapId GetFirstBitmapIdWithType(Assets& assets, AssetTypeID typeId) {
	BitmapId id = { _GetFirstAssetIdWithType(assets, typeId) };
	return id;
}

inline
BitmapId GetRandomBitmapId(Assets& assets, AssetTypeID typeId, RandomSeries& series) {
	BitmapId id = { _GetRandomAssetId(assets, typeId, series) };
	return id;
}

inline
BitmapId GetBestFitBitmapId(Assets& assets, AssetTypeID typeId, AssetFeatures match, AssetFeatures weight, f32 halfPeriod) {
	BitmapId id = { _GetBestFitAssetId(assets, typeId, match, weight, halfPeriod) };
	return id;
}

inline
bool NeedsFetching(Asset& asset) {
	return asset.state == AssetState::NotReady;
}

struct LoadAssetTaskArgs {
	PlatformFileHandle* source;
	void* buffer;
	u32 offset;
	u32 size;
	TaskWithMemory* task;
	AssetState* state;
};

internal
void LoadAssetBackgroundTask(void* data) {
	// TODO: Can I do that without opening/closing file all the time?
	LoadAssetTaskArgs* args = ptrcast(LoadAssetTaskArgs, data);
	Platform->FileRead(args->source, args->offset, args->size, args->buffer);
	if (!Platform->FileErrors(args->source)) {
		WriteCompilatorFence;
		*args->state = AssetState::Ready;
		EndBackgroundTask(args->task);
	}
	else {
		WriteCompilatorFence;
		Assert(!"Something bad happened with our file during the program, it shouldn't happen!");
		*args->state = AssetState::NotReady;
	}
}

inline
AssetMemoryBlock* TryMergeMemoryBlocks(AssetMemoryBlock* prev, AssetMemoryBlock* block) {
	if ((prev->flags & AssetMemory_BlockUsed) ||
		(block->flags & AssetMemory_BlockUsed)
		) {
		return block;
	}
	prev->size += block->size + sizeof(AssetMemoryBlock);
	prev->flags |= block->flags;
	block->prev->next = block->next;
	block->next->prev = block->prev;
	return prev;
}

inline
void ReleaseAssetMemory(Assets& assets, void* memory, u32 size) {
#if 0
	// This is OS allocation path
	if (memory) {
		Platform->MemoryFree(memory);
		assets.totalMemoryUsed -= size;
	}
#else
	AssetMemoryBlock* block = ptrcast(AssetMemoryBlock, memory) - 1;
	// Assert that next and prev blocks are determined by the sizes of the blocks
	Assert(((uptr)(block + 1) + block->size + size) == (uptr)block->next);
	block->flags &= ~AssetMemory_BlockUsed;
	block->size += size;

	if (block->prev != &assets.memorySentinel) {
		block = TryMergeMemoryBlocks(block->prev, block);
	}
	TryMergeMemoryBlocks(block, block->next);
#endif
}

inline
void EvictAssetsIfNeccessary(Assets& assets, u32 requestedSize) {
	while (assets.totalMemoryUsed + requestedSize > assets.totalMemoryMax) {
		AssetMemoryHeader* leastUsed = assets.lruSentinel.prev;
		while (leastUsed != &assets.lruSentinel) {
			Assert(leastUsed->assetIndex);
			Asset* asset = GetAsset(assets, leastUsed->assetIndex);
			if (asset->state == AssetState::Ready) {
				Assert(asset->memory == leastUsed);
				RemoveMemoryHeaderFromList(asset->memory);
				ReleaseAssetMemory(assets, asset->memory, asset->memory->totalSize);
				asset->memory = 0;
				asset->state = AssetState::NotReady;
				break;
			}
			leastUsed = leastUsed->prev;
		}
	}
}

inline
AssetMemoryBlock* FindMemoryBlockWithSize(Assets& assets, u32 size) {
	for (AssetMemoryBlock* block = assets.memorySentinel.next;
		block != &assets.memorySentinel;
		block = block->next
		) {
		if ((block->flags & AssetMemory_BlockUsed) ||
			(block->size < size)) 
		{
			continue;
		}
		return block;
	}
	return 0;
}

inline
void InsertNewMemoryBlock(AssetMemoryBlock* prev, void* memory, u32 size) {
	AssetMemoryBlock* block = ptrcast(AssetMemoryBlock, memory);
	block->flags = 0;
	block->size = size - sizeof(AssetMemoryBlock);
	block->prev = prev;
	block->next = prev->next;
	block->prev->next = block;
	block->next->prev = block;
}

internal
void* AcquireAssetMemory(Assets& assets, u32 size) {
#if 0
	// NOTE: OS allocation path
	EvictAssetsIfNeccessary(assets, size);
	void* result = Platform->MemoryAllocate(size);
	if (result) {
		assets.totalMemoryUsed += size;
	}
#else
	void* result = 0;
	for (;;) {
		AssetMemoryBlock* block = FindMemoryBlockWithSize(assets, size);
		if (block) {
			Assert(size <= block->size);
			assets.totalMemoryUsed += size;

			result = ptrcast(u8, block + 1);
			block->flags |= AssetMemory_BlockUsed;
			u32 remainingSize = block->size - size;
			u32 minimumBlockSize = 4096;
			if (remainingSize > minimumBlockSize) {
				block->size = 0;
				InsertNewMemoryBlock(block, ptrcast(u8, result) + size, remainingSize);
			}
			else {
				block->size = remainingSize;
			}
			break;
		}

		// NOTE: We don't have enough space for allocation. Evict lru asset
		AssetMemoryHeader* leastUsed = assets.lruSentinel.prev;
		while (leastUsed != &assets.lruSentinel) {
			Assert(leastUsed->assetIndex);
			Asset* asset = GetAsset(assets, leastUsed->assetIndex);
			if (asset->state == AssetState::Ready) {
				Assert(asset->memory == leastUsed);
				RemoveMemoryHeaderFromList(asset->memory);
				ReleaseAssetMemory(assets, asset->memory, asset->memory->totalSize);
				asset->memory = 0;
				asset->state = AssetState::NotReady;
				break;
			}
			leastUsed = leastUsed->prev;
		}
	}
	
#endif
	return result;
}

internal
bool PrefetchBitmap(Assets& assets, BitmapId bid) {
	Asset& asset = assets.assets[bid.id];
	if (!IsValid(bid) || !NeedsFetching(asset)) {
		return false;
	}
	TaskWithMemory* task = TryBeginBackgroundTask(assets.tranState);
	if (!task) {
		return false;
	}
	AssetFileBitmapInfo* metadata = &GetAssetMetadata(assets, asset)->_bitmapInfo;
	u32 assetSize = metadata->pitch * metadata->height * BITMAP_BYTES_PER_PIXEL;
	u32 allocSize = assetSize + sizeof(AssetMemoryHeader);
	asset.memory = ptrcast(AssetMemoryHeader, AcquireAssetMemory(assets, allocSize));
	asset.memory->bitmap.align = metadata->alignment;
	asset.memory->bitmap.height = metadata->height;
	asset.memory->bitmap.width = metadata->width;
	asset.memory->bitmap.pitch = metadata->pitch;
	asset.memory->bitmap.widthOverHeight = f4(metadata->width) / f4(metadata->height);
	asset.memory->bitmap.data = ptrcast(u32, asset.memory + 1);
	asset.memory->type = AssetData_Bitmap;
	asset.memory->assetIndex = bid.id;
	asset.memory->totalSize = allocSize;
	asset.state = AssetState::Pending;
	AddMemoryHeaderToList(assets, asset.memory);

	LoadAssetTaskArgs* args = PushStructSize(task->arena, LoadAssetTaskArgs);
	args->source = GetAssetSource(assets, asset.fileSourceIndex);
	args->offset = metadata->dataOffset;
	args->size = assetSize;
	args->buffer = asset.memory->bitmap.data;
	args->task = task;
	args->state = &asset.state;

	WriteCompilatorFence;
	Platform->QueuePushTask(assets.tranState->lowPriorityQueue, LoadAssetBackgroundTask, args);
	return true;
}

internal
bool PrefetchSound(Assets& assets, SoundId sid) {
	Asset& asset = assets.assets[sid.id];
	if (!IsValid(sid) || !NeedsFetching(asset)) {
		return false;
	}
	TaskWithMemory* task = TryBeginBackgroundTask(assets.tranState);
	if (!task) {
		return false;
	}

	AssetFileSoundInfo* metadata = &GetAssetMetadata(assets, asset)->_soundInfo;
	u32 assetSize = (metadata->sampleCount + SOUND_CHUNK_SAMPLE_OVERLAP) * 
		metadata->nChannels * sizeof(f32);
	u32 allocSize = assetSize + sizeof(AssetMemoryHeader);
	asset.memory = ptrcast(AssetMemoryHeader, AcquireAssetMemory(assets, allocSize));
	asset.memory->sound.nChannels = metadata->nChannels;
	asset.memory->sound.sampleCount = metadata->sampleCount;
	asset.memory->sound.samples[0] = ptrcast(f32, asset.memory + 1);
	asset.memory->sound.samples[1] = asset.memory->sound.samples[0] + metadata->sampleCount + SOUND_CHUNK_SAMPLE_OVERLAP;
	asset.memory->type = AssetData_Sound;
	asset.memory->assetIndex = sid.id;
	asset.memory->totalSize = allocSize;
	asset.state = AssetState::Pending;
	AddMemoryHeaderToList(assets, asset.memory);

	LoadAssetTaskArgs* args = PushStructSize(task->arena, LoadAssetTaskArgs);
	args->source = GetAssetSource(assets, asset.fileSourceIndex);
	args->offset = metadata->samplesOffset[0];
	args->size = assetSize;
	args->buffer = asset.memory->sound.samples[0];
	args->task = task;
	args->state = &asset.state;
	WriteCompilatorFence;
	Platform->QueuePushTask(assets.tranState->lowPriorityQueue, LoadAssetBackgroundTask, args);
	return true;
}

inline
bool LoadIfNotAllAssetsAreReady(Assets& assets, AssetTypeID typeId) {
	AssetGroup* group = GetAssetGroup(assets, typeId);
	bool ready = true;
	for (u32 assetIndex = group->firstAssetIndex;
		assetIndex < group->onePastLastAssetIndex;
		assetIndex++)
	{
		Asset* asset = GetAsset(assets, assetIndex);
		if (!IsReady(asset)) {
			ready = false;
			if (group->type == AssetGroup_Bitmap) {
				PrefetchBitmap(assets, { assetIndex });
			}
			else {
				PrefetchSound(assets, { assetIndex });
			}
		}
	}
	return ready;
}

internal
void AllocateAssets(TransientState* tranState) {
	u32 assetsCount = 0;
	PlatformFileGroup* fileGroup = Platform->FileOpenAllWithExtension("assf");
	// NOTE: Read how many assets do we have at all first!
	for (u32 fileIndex = 0; fileIndex < fileGroup->count; fileIndex++) {
		PlatformFileHandle* file = *(fileGroup->files + fileIndex);
		AssetFileHeader header;
		Platform->FileRead(file, 0, sizeof(header), &header);
		if (Platform->FileErrors(file) ||
			header.magicString != EAF_MAGIC_STRING('a', 's', 's', 'f') ||
			header.version != 0) {
			// TODO: Inform user about IO error
			continue;
		}
		// NOTE: First asset is null asset, it shouldn't be taken into account
		assetsCount += header.assetsCount - 1;
	}
	Assets& assets = tranState->assets;
	u32 memoryForAssetsSize = MB(6);
	assets.tranState = tranState;
	assets.assetCount = assetsCount + 1;
	assets.assets = PushArray(tranState->arena, assets.assetCount, Asset);
	assets.features = PushArray(tranState->arena, assets.assetCount, AssetFeatures);
	assets.metadatas = PushArray(tranState->arena, assets.assetCount, AssetMetadata);
	assets.sources = fileGroup;
	assets.totalMemoryMax = memoryForAssetsSize;
	assets.totalMemoryUsed = 0;
	assets.lruSentinel.next = &assets.lruSentinel;
	assets.lruSentinel.prev = &assets.lruSentinel;
	assets.memorySentinel.next = &assets.memorySentinel;
	assets.memorySentinel.prev = &assets.memorySentinel;
	assets.memorySentinel.size = 0;
	assets.memorySentinel.flags = 0;
	InsertNewMemoryBlock(&assets.memorySentinel,
		PushSize(tranState->arena, memoryForAssetsSize), memoryForAssetsSize);

	TemporaryMemory scratchMemory = BeginTempMemory(tranState->arena);
	AssetFileHeader* headers = PushArray(tranState->arena, fileGroup->count, AssetFileHeader);
	// NOTE: Read the headers first!
	for (u32 fileIndex = 0; fileIndex < fileGroup->count; fileIndex++) {
		AssetFileHeader* header = headers + fileIndex;
		PlatformFileHandle* file = *(fileGroup->files + fileIndex);
		Platform->FileRead(file, 0, sizeof(AssetFileHeader), header);
		if (Platform->FileErrors(file) ||
			header->magicString != EAF_MAGIC_STRING('a', 's', 's', 'f') ||
			header->version != 0) {
			// TODO: Inform user about IO error
			continue;
		}
	}

	// NOTE: The first asset is NULL asset, so readAssetsCount must start from first NON NULL asset
	// NOTE: Also, first group in the file is NULL group, so start from NON NULL group
	u32 readAssetsCount = 1;
	for (u32 groupIndex = 1; groupIndex < Asset_Count; groupIndex++) {
		AssetGroup* combinedAssetGroup = assets.groups + groupIndex;
		AssetGroup* fileAssetGroup = PushStructSize(tranState->arena, AssetGroup);

		for (u32 fileIndex = 0; fileIndex < fileGroup->count; fileIndex++) {
			PlatformFileHandle* file = *(fileGroup->files + fileIndex);
			AssetFileHeader* header = headers + fileIndex;

			Platform->FileRead(
				file,
				u4(header->assetGroupsOffset) + sizeof(AssetGroup) * groupIndex,
				sizeof(AssetGroup),
				fileAssetGroup
			);
#if 0	// Asset group might be actually empty
			Assert(fileAssetGroup->firstAssetIndex != 0);
			Assert(fileAssetGroup->onePastLastAssetIndex != 0);
#endif
			Assert(fileAssetGroup->firstAssetIndex < header->assetsCount);
			u32 fileAssetCountInGroup = fileAssetGroup->onePastLastAssetIndex - fileAssetGroup->firstAssetIndex;

			Platform->FileRead(
				file,
				u4(header->featuresOffset) + sizeof(AssetFeatures) * fileAssetGroup->firstAssetIndex,
				sizeof(AssetFeatures) * fileAssetCountInGroup,
				assets.features + readAssetsCount
			);
			Platform->FileRead(
				file,
				u4(header->assetMetadatasOffset) + sizeof(AssetMetadata) * fileAssetGroup->firstAssetIndex,
				sizeof(AssetMetadata) * fileAssetCountInGroup,
				assets.metadatas + readAssetsCount
			);
			if (Platform->FileErrors(file)) {
				// TODO: Inform user about IO error
				continue;
			}
			for (u32 baseIndex = 0; baseIndex < fileAssetCountInGroup; baseIndex++) {
				Asset* dstAsset = assets.assets + readAssetsCount + baseIndex;
				dstAsset->fileSourceIndex = fileIndex;
				dstAsset->metadataId = readAssetsCount + baseIndex;
			}
			u32 combinedAssetGroupCount = combinedAssetGroup->onePastLastAssetIndex - combinedAssetGroup->firstAssetIndex;
			if (combinedAssetGroupCount == 0) {
				combinedAssetGroup->firstAssetIndex = readAssetsCount;
				combinedAssetGroup->onePastLastAssetIndex = readAssetsCount + fileAssetCountInGroup;
				combinedAssetGroup->type = fileAssetGroup->type;
			}
			else {
				combinedAssetGroup->onePastLastAssetIndex += fileAssetCountInGroup;
			}
			readAssetsCount += fileAssetCountInGroup;
		}
	}
	Assert(assets.assetCount == readAssetsCount);

	// NOTE: Keep files open!
	EndTempMemory(scratchMemory);
	CheckArena(tranState->arena);
}