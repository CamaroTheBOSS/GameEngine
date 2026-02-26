#include "engine.h"

u32 debugLruCount = 0;
u32 debugMemBlockCount = 0;

inline
void BeginAssetMemoryLock(Assets& assets) {
	while (AtomicCompareExchange(&assets.memoryLock, 1, 0) != 0) {};
}

inline
void EndAssetMemoryLock(Assets& assets) {
	AtomicCompareExchange(&assets.memoryLock, 0, 1);
}

inline
void AddMemoryHeaderToList(Assets& assets, AssetMemoryHeader* header) {
	// NOTE: Needs asset lock
	header->next = assets.lruSentinel.next;
	header->prev = &assets.lruSentinel;
	header->next->prev = header;
	header->prev->next = header;
	debugLruCount++;
}

inline
void LockedAddMemoryHeaderToList(Assets& assets, AssetMemoryHeader* header) {
	BeginAssetMemoryLock(assets);
	AddMemoryHeaderToList(assets, header);
	EndAssetMemoryLock(assets);
}

inline
void RemoveMemoryHeaderFromList(AssetMemoryHeader* header) {
	// NOTE: Needs asset lock
	header->next->prev = header->prev;
	header->prev->next = header->next;
	debugLruCount--;
}

inline
void LockedRemoveMemoryHeaderFromList(Assets& assets, AssetMemoryHeader* header) {
	BeginAssetMemoryLock(assets);
	RemoveMemoryHeaderFromList(header);
	EndAssetMemoryLock(assets);
}

inline
void LockedMoveMemoryHeaderToFront(Assets& assets, AssetMemoryHeader* header) {
	BeginAssetMemoryLock(assets);
	RemoveMemoryHeaderFromList(header);
	AddMemoryHeaderToList(assets, header);
	EndAssetMemoryLock(assets);
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

internal
Asset* AcquireAsset(Assets& assets, u32 aid, GenerationId gid) {
	Assert(gid.id);
	Asset* asset = GetAsset(assets, aid);
	// TODO: Should it be more finegrained lock on per asset basis?
	BeginAssetMemoryLock(assets);
	if (asset->state == AssetState_Ready) {
		if (asset && asset->memory) {
			if (asset->memory->generationId < gid.id) {
				asset->memory->generationId = gid.id;
			}
			RemoveMemoryHeaderFromList(asset->memory);
			AddMemoryHeaderToList(assets, asset->memory);
		}
	}
	EndAssetMemoryLock(assets);
	return asset;
}

inline
LoadedBitmap* GetBitmap(Assets& assets, BitmapId bid, GenerationId gid) {
	Asset* asset = AcquireAsset(assets, bid.id, gid);
	if (!asset || !asset->memory) {
		return 0;
	}
	Assert(asset->memory->type == AssetData_Bitmap);
	LoadedBitmap* bitmap = &asset->memory->bitmap;
	return bitmap;
}

inline
LoadedSound* GetSound(Assets& assets, SoundId sid, GenerationId gid) {
	Asset* asset = AcquireAsset(assets, sid.id, gid);
	if (!asset || !asset->memory) {
		return 0;
	}
	Assert(asset->memory->type == AssetData_Sound);
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

internal
GenerationId NewGenerationId(Assets& assets) {
	BeginAssetMemoryLock(assets);
	Assert(assets.nextGenerationId.id < U32_MAX);
	Assert(assets.inFlightGenerationCount < ArrayCount(assets.inFlightGenerations));
	GenerationId result = {};
	result.id = ++assets.nextGenerationId.id;
	assets.inFlightGenerations[assets.inFlightGenerationCount++] = result;
	EndAssetMemoryLock(assets);
	return result;
}


internal
void FinishGeneration(Assets& assets, GenerationId gid) {
	BeginAssetMemoryLock(assets);
	for (u32 index = 0; index < assets.inFlightGenerationCount; index++) {
		if (assets.inFlightGenerations[index].id == gid.id) {
			assets.inFlightGenerations[index] = assets.inFlightGenerations[--assets.inFlightGenerationCount];
			break;
		}
	}
	EndAssetMemoryLock(assets);
}

internal
bool GenerationHasCompleted(Assets& assets, Asset* asset) {
	//NOTE: Must be locked
	for (u32 index = 0; index < assets.inFlightGenerationCount; index++) {
		if (assets.inFlightGenerations[index].id <= asset->memory->generationId) {
			return false;
		}
	}
	return true;
}

inline
bool NeedsFetching(Asset& asset) {
	return asset.state == AssetState_NotReady;
}

struct LoadAssetTaskArgs {
	PlatformFileHandle* source;
	void* buffer;
	u32 offset;
	u32 size;
	TaskWithMemory* task;
	u32* state;
};

internal
void LoadAssetBackgroundTask(void* data) {
	// TODO: Can I do that without opening/closing file all the time?
	LoadAssetTaskArgs* args = ptrcast(LoadAssetTaskArgs, data);
	Platform->FileRead(args->source, args->offset, args->size, args->buffer);
	if (!Platform->FileErrors(args->source)) {
		WriteCompilatorFence;
		*args->state = AssetState_Ready;
		if (args->task) {
			EndBackgroundTask(args->task);
		}
	}
	else {
		WriteCompilatorFence;
		Assert(!"Something bad happened with our file during the program, it shouldn't happen!");
		*args->state = AssetState_NotReady;
	}
}

inline
AssetMemoryBlock* TryMergeMemoryBlocks(AssetMemoryBlock* prev, AssetMemoryBlock* block) {
	// NOTE: Must be locked
	if ((prev->flags & AssetMemory_BlockUsed) ||
		(block->flags & AssetMemory_BlockUsed)
		) {
		return block;
	}
	Assert(prev->remainingSize == prev->totalSize);
	Assert(block->remainingSize == block->totalSize);	
	prev->totalSize += block->totalSize + sizeof(AssetMemoryBlock);
	prev->remainingSize = prev->totalSize;
	prev->flags |= block->flags;
	block->prev->next = block->next;
	block->next->prev = block->prev;
	debugMemBlockCount--;
	return prev;
}

inline bool CheckMemoryBlockBeforeRelease(Assets& assets, AssetMemoryBlock* block, u32 memoryReleaseSize) {
	u32 totalAfterRelease = block->remainingSize + memoryReleaseSize;
	Assert(totalAfterRelease == block->totalSize);
	Assert(block != &assets.memorySentinel);

	uptr blockStart = (uptr)block;
	uptr nextBlockStart = (uptr)(block->next);
	uptr blockMemoryStart = (uptr)(block + 1);
	uptr prevBlockMemoryStart = (uptr)(block->prev + 1);
	uptr prevBlockTotalSize = block->prev->totalSize;

	// Assert that next and prev blocks are determined by the sizes of the blocks
	if (block->next != &assets.memorySentinel) {
		Assert((blockMemoryStart + totalAfterRelease) == nextBlockStart);
	}
	if (block->prev != &assets.memorySentinel) {
		Assert((prevBlockMemoryStart + prevBlockTotalSize) == blockStart);
	}
	return true;
}

inline
AssetMemoryBlock* ReleaseAssetMemory(Assets& assets, void* memory, u32 size) {
	// NOTE: Must be locked
	AssetMemoryBlock* block = ptrcast(AssetMemoryBlock, memory) - 1;
	Assert(CheckMemoryBlockBeforeRelease(assets, block, size));
	block->flags &= ~AssetMemory_BlockUsed;
	block->remainingSize += size;

	if (block->prev != &assets.memorySentinel) {
		block = TryMergeMemoryBlocks(block->prev, block);
	}
	if (block->next != &assets.memorySentinel) {
		TryMergeMemoryBlocks(block, block->next);
	}
	return block;
}

inline
AssetMemoryBlock* FindMemoryBlockWithSize(Assets& assets, u32 size) {
	// NOTE: Must be locked
	// TODO: This is exteremely unefficient for huge amount of assets loaded in memory
	// Every time we allocate new asset we need to traverse through thousands of links
	// to find a block. The search system should be created on top of that memory system.
	for (AssetMemoryBlock* block = assets.memorySentinel.next;
		block != &assets.memorySentinel;
		block = block->next
		) {
		if ((block->flags & AssetMemory_BlockUsed) ||
			(block->remainingSize < size)) 
		{
			continue;
		}
		return block;
	}
	return 0;
}

inline
void InsertNewMemoryBlock(AssetMemoryBlock* prev, void* memory, u32 size) {
	// NOTE: Must be locked
	AssetMemoryBlock* block = ptrcast(AssetMemoryBlock, memory);
	block->flags = 0;
	block->remainingSize = size - sizeof(AssetMemoryBlock);
	block->totalSize = size - sizeof(AssetMemoryBlock);
	block->prev = prev;
	block->next = prev->next;
	block->prev->next = block;
	block->next->prev = block;
	debugMemBlockCount++;
}

internal
void* AcquireAssetMemory(Assets& assets, u32 size) {
	void* result = 0;
	BeginAssetMemoryLock(assets);
	AssetMemoryBlock* block = FindMemoryBlockWithSize(assets, size);
	for (;;) {
		if (block && size <= block->remainingSize) {

			result = ptrcast(u8, block + 1);
			block->flags |= AssetMemory_BlockUsed;
			u32 newRemainingSize = block->remainingSize - size;
			u32 minimumBlockSize = 4096;
			if (newRemainingSize > minimumBlockSize) {
				block->totalSize = size;
				block->remainingSize = 0;
				
				InsertNewMemoryBlock(block, ptrcast(u8, result) + size, newRemainingSize);
			}
			else {
				block->remainingSize = newRemainingSize;
			}
			break;
		}

		// NOTE: We don't have enough space for allocation. Evict lru asset
		AssetMemoryHeader* leastUsed = assets.lruSentinel.prev;
		bool evicted = false;
		while (leastUsed != &assets.lruSentinel) {
			Assert(leastUsed->assetIndex);
			Asset* asset = GetAsset(assets, leastUsed->assetIndex);
			if (GenerationHasCompleted(assets, asset) &&
				AtomicCompareExchange(&asset->state, AssetState_NotReady, AssetState_Ready) == AssetState_Ready
				) {
				Assert(asset->memory == leastUsed);
				RemoveMemoryHeaderFromList(leastUsed);
				// NOTE: When we don't find any block and we evicted one asset, only the block
				// from evicted asset can be used to check whether more assets needs to be evicted
				// or allocation can take place
				block = ReleaseAssetMemory(assets, leastUsed, leastUsed->totalSize);
				asset->memory = 0;
				evicted = true;
				break;
			}
			leastUsed = leastUsed->prev;
		}
		Assert(evicted && "Holy crap, we ran out of memory and we cannot evict more assets!");
		break;
	}
	EndAssetMemoryLock(assets);
	return result;
}

internal
bool PrefetchBitmap(Assets& assets, BitmapId bid, bool immediate) {
	Asset& asset = assets.assets[bid.id];
	if (!IsValid(bid) || !NeedsFetching(asset)) {
		return false;
	}
	TaskWithMemory* task = 0;
	if (!immediate) {
		AssertMainThread;
		task = TryBeginBackgroundTask(assets.tranState);
		if (!task) {
			return false;
		}
	}
#if 1
	if (AtomicCompareExchange(&asset.state, AssetState_Pending, AssetState_NotReady) != AssetState_NotReady) {
		if (task) {
			EndBackgroundTask(task);
		}
		return false;
	}
#endif
	asset.state = AssetState_Pending;
	WriteCompilatorFence;
	AssetFileBitmapInfo* metadata = &GetAssetMetadata(assets, asset)->_bitmapInfo;
	u32 assetSize = metadata->pitch * metadata->height;
	u32 allocSize = assetSize + sizeof(AssetMemoryHeader);
	asset.memory = ptrcast(AssetMemoryHeader, AcquireAssetMemory(assets, allocSize));
	if (!asset.memory) {
		// Note AcquireAssetMemory might fail, in such case we need to gracefully fallback 
		// in callee code
		if (task) {
			EndBackgroundTask(task);
		}
		WriteCompilatorFence;
		asset.state = AssetState_NotReady;
		return false;
	}
	asset.memory->bitmap.align = metadata->alignment;
	asset.memory->bitmap.height = metadata->height;
	asset.memory->bitmap.width = metadata->width;
	asset.memory->bitmap.pitch = metadata->pitch;
	asset.memory->bitmap.widthOverHeight = f4(metadata->width) / f4(metadata->height);
	asset.memory->bitmap.data = ptrcast(u32, asset.memory + 1);
	asset.memory->type = AssetData_Bitmap;
	asset.memory->assetIndex = bid.id;
	asset.memory->totalSize = allocSize;
	asset.memory->generationId = 0;
	
	LockedAddMemoryHeaderToList(assets, asset.memory);

	LoadAssetTaskArgs stackDeclaration;
	LoadAssetTaskArgs* args = 0;
	if (immediate) {
		stackDeclaration = {};
		args = &stackDeclaration;
	}
	else {
		AssertMainThread;
		args = PushStructSize(task->arena, LoadAssetTaskArgs);
	}
	args->source = GetAssetSource(assets, asset.fileSourceIndex);
	args->offset = metadata->dataOffset;
	args->size = assetSize;
	args->buffer = asset.memory->bitmap.data;
	args->task = task;
	args->state = &asset.state;

	if (immediate) {
		LoadAssetBackgroundTask(args);
	}
	else {
		WriteCompilatorFence;
		AssertMainThread;
		Platform->QueuePushTask(assets.tranState->lowPriorityQueue, LoadAssetBackgroundTask, args);
	}
	return true;
}

internal
bool PrefetchSound(Assets& assets, SoundId sid, bool immediate) {
	Asset& asset = assets.assets[sid.id];
	if (!IsValid(sid) || !NeedsFetching(asset)) {
		return false;
	}
	TaskWithMemory* task = 0;
	if (!immediate) {
		task = TryBeginBackgroundTask(assets.tranState);
		if (!task) {
			return false;
		}
	}
#if 1
	if (AtomicCompareExchange(&asset.state, AssetState_Pending, AssetState_NotReady) != AssetState_NotReady) {
		if (task) {
			EndBackgroundTask(task);
		}
		return false;
	}
#endif
	asset.state = AssetState_Pending;
	WriteCompilatorFence;
	AssetFileSoundInfo* metadata = &GetAssetMetadata(assets, asset)->_soundInfo;
	u32 assetSize = (metadata->sampleCount + SOUND_CHUNK_SAMPLE_OVERLAP) * 
		metadata->nChannels * sizeof(f32);
	u32 allocSize = assetSize + sizeof(AssetMemoryHeader);
	asset.memory = ptrcast(AssetMemoryHeader, AcquireAssetMemory(assets, allocSize));
	if (!asset.memory) {
		// Note AcquireAssetMemory might fail, in such case we need to gracefully fallback 
		// in callee code
		if (task) {
			EndBackgroundTask(task);
		}
		WriteCompilatorFence;
		asset.state = AssetState_NotReady;
		return false;
	}
	asset.memory->sound.nChannels = metadata->nChannels;
	asset.memory->sound.sampleCount = metadata->sampleCount;
	asset.memory->sound.samples[0] = ptrcast(f32, asset.memory + 1);
	asset.memory->sound.samples[1] = asset.memory->sound.samples[0] + metadata->sampleCount + SOUND_CHUNK_SAMPLE_OVERLAP;
	asset.memory->type = AssetData_Sound;
	asset.memory->assetIndex = sid.id;
	asset.memory->totalSize = allocSize;
	asset.memory->generationId = 0;
	LockedAddMemoryHeaderToList(assets, asset.memory);

	LoadAssetTaskArgs stackDeclaration;
	LoadAssetTaskArgs* args = 0;
	if (immediate) {
		stackDeclaration = {};
		args = &stackDeclaration;
	}
	else {
		args = PushStructSize(task->arena, LoadAssetTaskArgs);
	}
	args->source = GetAssetSource(assets, asset.fileSourceIndex);
	args->offset = metadata->samplesOffset[0];
	args->size = assetSize;
	args->buffer = asset.memory->sound.samples[0];
	args->task = task;
	args->state = &asset.state;

	if (immediate) {
		LoadAssetBackgroundTask(&args);
	}
	else {
		WriteCompilatorFence;
		Platform->QueuePushTask(assets.tranState->lowPriorityQueue, LoadAssetBackgroundTask, args);
	}
	return true;
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
	assets.nextGenerationId.id = 0;
	assets.inFlightGenerationCount = 0;
	assets.memoryLock = 0;
	assets.tranState = tranState;
	assets.assetCount = assetsCount + 1;
	assets.assets = PushArray(tranState->arena, assets.assetCount, Asset);
	assets.features = PushArray(tranState->arena, assets.assetCount, AssetFeatures);
	assets.metadatas = PushArray(tranState->arena, assets.assetCount, AssetMetadata);
	assets.sources = fileGroup;
	assets.lruSentinel.next = &assets.lruSentinel;
	assets.lruSentinel.prev = &assets.lruSentinel;
	assets.memorySentinel.next = &assets.memorySentinel;
	assets.memorySentinel.prev = &assets.memorySentinel;
	assets.memorySentinel.remainingSize = 0;
	assets.memorySentinel.totalSize = 0;
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