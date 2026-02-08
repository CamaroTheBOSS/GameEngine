#include "engine.h"
#include "engine_world.cpp"
#include "engine_simulation.cpp"
#include "engine_rand.cpp"
#include "engine_render.cpp"

internal
void RenderSoundToBuffer(AudioState& audio, Assets& assets, SoundData& dst) {
	constexpr u32 nChannels = 2;
	TemporaryMemory mixerMemory = BeginTempMemory(audio.arena);
	f32* mixedSamples[nChannels] = {};
	
	u32 outBufferSampleCount = dst.nSamples;
	u32 maxIter = outBufferSampleCount >> 3;
	Assert((outBufferSampleCount & 7) == 0);
	__m256 zero = _mm256_set1_ps(0.f);
	for (u32 channel = 0; channel < nChannels; channel++) {
		mixedSamples[channel] = PushArray(audio.arena, outBufferSampleCount, f32, 32);
		f32* data = mixedSamples[channel];
		for (u32 i = 0; i < maxIter; i++) {
			_mm256_store_ps(data, zero);
			data += 8;
		}
	}
	PlayingSound* prevSound = 0;
	PlayingSound* currSound = audio.playingSounds;
	u32 destCurrentSample = 0;
	while (currSound) {
		// TODO: Hunt for this assertion and check whether it is still needed, probably not
		// Assert(destCurrentSample < outBufferSampleCount);
		Asset* asset = GetAsset(assets, currSound->soundId.id);
		SoundInfo* soundInfo = &asset->soundInfo;
		if (!IsReady(asset)) {
			// TESTING NOTE: That assert is handy in development, comment it out when want to test
			// whether asset system is resistent on lack of assets loaded
#if 1
			// Note: Only first chunk shouldn't be ready, the rest needs to be here on time to avoid
			// clicking!
			Assert(soundInfo->firstSampleIndex == 0);
#endif
			prevSound = currSound;
			currSound = currSound->next;
			continue;
		}
		SoundId nextInChain = { 0 };
		if (asset->soundInfo.chain.op == SoundChain::Advance) {
			nextInChain.id = currSound->soundId.id + asset->soundInfo.chain.count;
		}
		PrefetchSound(assets, nextInChain);
		LoadedSound* assetSound = &asset->sound;
		PlayingSound* nextSound = currSound->next;

		f32 remainingSamples = f4(outBufferSampleCount - destCurrentSample);
		f32 samplesToPlay = (f4(assetSound->sampleCount) - currSound->currentSample) / currSound->pitch;
		if (samplesToPlay > remainingSamples) {
			samplesToPlay = remainingSamples;
		}

		f32* src[nChannels] = {};
		f32* dest[nChannels] = {};
		__m256 startVolume[nChannels] = {};
		__m256 volumeChangeSpeed[nChannels] = {};
		bool needsRepetition = false;
		V2 volumeSpeed = currSound->volumeChangeSpeed;
		V2 diffVolume = currSound->requestedVolume - currSound->currentVolume;
		for (u32 channel = 0; channel < nChannels; channel++) {
			src[channel] = assetSound->samples[channel] + FloorF32ToU32(currSound->currentSample);
			dest[channel] = mixedSamples[channel] + destCurrentSample;
			startVolume[channel] = _mm256_set1_ps(currSound->currentVolume.E[channel]);
			volumeChangeSpeed[channel] = _mm256_set1_ps(currSound->volumeChangeSpeed.E[channel]);

			// TODO: Can it be done with no epsilons?
			f32 epsilon = 0.00001f;
			if (Abs(diffVolume.E[channel]) > epsilon && Abs(volumeSpeed.E[channel]) > epsilon) {
				f32 samplesToEndVolume = diffVolume.E[channel] / volumeSpeed.E[channel];
				i32 samples = RoundF32ToI32(samplesToEndVolume);
				// TODO: Hunt for this assertion, it will fire when volumeSpeed is very very small
				Assert(samples >= 0);
				if (samples == 0) {
					currSound->volumeChangeSpeed.E[channel] = 0;
					currSound->requestedVolume.E[channel] = currSound->currentVolume.E[channel] + f4(samplesToPlay) * volumeSpeed.E[channel];
				}
				else if (samples > 0 && samplesToPlay > f4(samples)) {
					samplesToPlay = f4(samples);
					needsRepetition = true;
				}
			}
		}
		
		__m256 sampleIndexes = _mm256_setr_ps(0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f);
		__m256 pitch8 = _mm256_set1_ps(currSound->pitch);
		__m256i ones = _mm256_set1_epi32(1);
		__m256 eight = _mm256_set1_ps(8);
		__m256 samplesToPlay8 = _mm256_set1_ps(samplesToPlay);
		for (f32 index = 0; index < samplesToPlay; index += 8) {
			__m256 samples = _mm256_mul_ps(sampleIndexes, pitch8);
			__m256i samplesInt = _mm256_cvttps_epi32(samples);
			__m256i samplesIntPlus1 = _mm256_add_epi32(samplesInt, ones);
			__m256 samplesFrac = _mm256_sub_ps(samples, _mm256_cvtepi32_ps(samplesInt));
			__m256 writeMask = _mm256_cmp_ps(sampleIndexes, samplesToPlay8, _CMP_LT_OQ);
			for (u32 channel = 0; channel < nChannels; channel++) {
				__m256 volume = _mm256_add_ps(startVolume[channel], _mm256_mul_ps(sampleIndexes, volumeChangeSpeed[channel]));
				__m256 srcSamples1 = _mm256_i32gather_ps(src[channel], samplesInt, 4);
				__m256 srcSamples2 = _mm256_i32gather_ps(src[channel], samplesIntPlus1, 4);
				__m256 destSamples = _mm256_load_ps(dest[channel]);
				__m256 output = _mm256_add_ps(srcSamples1, _mm256_mul_ps(samplesFrac, _mm256_sub_ps(srcSamples2, srcSamples1)));
				output = _mm256_add_ps(destSamples, _mm256_mul_ps(volume, output));
				output = _mm256_blendv_ps(destSamples, output, writeMask);
				_mm256_store_ps(dest[channel], output);
				dest[channel] += 8;
			}
			sampleIndexes = _mm256_add_ps(sampleIndexes, eight);
		}
		currSound->currentVolume += samplesToPlay * volumeSpeed;
		currSound->currentSample += samplesToPlay * currSound->pitch;
		bool soundChunkFinished = FloorF32ToU32(currSound->currentSample) >= assetSound->sampleCount;
		if (needsRepetition) {
			Assert(!soundChunkFinished);
			destCurrentSample += CeilF32ToU32(samplesToPlay);
			continue;
		}
		if (soundChunkFinished) {
			if (IsValid(nextInChain)) {
				currSound->soundId = nextInChain;
				currSound->currentSample -= soundInfo->chunkSampleCount;
				destCurrentSample += CeilF32ToU32(samplesToPlay);
				continue;
			}
			if (prevSound) {
				prevSound->next = currSound->next;
			}
			else {
				audio.playingSounds = currSound->next;
			}
			currSound->next = audio.freeListSounds;
			audio.freeListSounds = currSound;
		}
		else {
			prevSound = currSound;
		}
		currSound = nextSound;
		destCurrentSample = 0;
	}

	f32* out = ptrcast(f32, dst.data);
	f32* mixedC1 = mixedSamples[0];
	f32* mixedC2 = mixedSamples[1];
	for (u32 iter = 0; iter < maxIter; iter++) {
		__m256 samplesC1 = _mm256_load_ps(mixedC1);
		__m256 samplesC2 = _mm256_load_ps(mixedC2);
		__m256 unpackedLow = _mm256_unpacklo_ps(samplesC1, samplesC2);
		__m256 unpackedHigh = _mm256_unpackhi_ps(samplesC1, samplesC2);
		__m256 permutedLow = _mm256_permute2f128_ps(unpackedLow, unpackedHigh, 0b0011'0001);
		__m256 permutedHigh = _mm256_permute2f128_ps(unpackedLow, unpackedHigh, 0b0010'0000);
		_mm256_store_ps(out, permutedHigh);
		out += 8;
		_mm256_store_ps(out, permutedLow);
		out += 8;
		mixedC1 += 8;
		mixedC2 += 8;
	}
	EndTempMemory(mixerMemory);
}

internal
void RenderHitPoints(RenderGroup& group, Entity& entity, V3 center, V2 offset, f32 distBetween, f32 pointSize, V4 color) {
	V2 realOffset = (offset + V2{ -scast(f32, entity.hitPoints.count - 1) * scast(f32, distBetween + pointSize) / 2.f , 0.f });
	V2 pointSizeVec = V2{ pointSize, pointSize };
	for (u32 hitPointIndex = 0; hitPointIndex < entity.hitPoints.count; hitPointIndex++) {
		V3 rectCenter = center + V3{ realOffset.X, realOffset.Y, 0.f };
		PushRect(group, rectCenter, pointSizeVec, V2{0, 0}, color);
		realOffset.X += (distBetween + pointSize);
	}
}

struct FillGroundBufferTaskArgs {
	TaskWithMemory* task;
	WorldPosition chunkPos;
	V2 chunkSizeInMeters;
	Assets* assets;
	GroundBuffer* groundBuffer;
};

internal
void FillGroundBufferBackgroundTask(void* data) {
	FillGroundBufferTaskArgs* args = ptrcast(FillGroundBufferTaskArgs, data);
	TaskWithMemory* task = args->task;
	LoadedBitmap* buffer = &args->groundBuffer->buffer;
	RenderGroup group = AllocateRenderGroup(task->arena, u4(GetArenaFreeSpaceSize(task->arena)));
	f32 width = args->chunkSizeInMeters.X;
	f32 height = args->chunkSizeInMeters.Y;
	Assert(width == height);
	f32 metersToPixels = (buffer->width - 2) / width;
	group.projection = GetOrtographicProjection(buffer->width, buffer->height, metersToPixels);
	PushClearCall(group, V4{ 1, 0, 1, 1 });
	for (i32 chunkOffsetY = -1; chunkOffsetY <= 1; chunkOffsetY++) {
		for (i32 chunkOffsetX = -1; chunkOffsetX <= 1; chunkOffsetX++) {
			i32 chunkX = args->chunkPos.chunkX + chunkOffsetX;
			i32 chunkY = args->chunkPos.chunkY + chunkOffsetY;
			i32 chunkZ = args->chunkPos.chunkZ;
#if 0
			V4 color = V4{ 1, 0, 0, 1 };
			if (((chunkX % 2) == 1 && (chunkY % 2) == 1) || (((chunkX % 2) == 0) && ((chunkY % 2) == 0))) {
				color = V4{ 0, 1, 1, 1 };
			}
#else
			V4 color = { 1, 1, 1, 1 };
#endif
			u32 seed = 313 * chunkX + 217 * chunkY + 177 * chunkZ;
			RandomSeries series = RandomSeed(seed);
			for (u32 bmpIndex = 0; bmpIndex < 100; bmpIndex++) {
				V3 position = V3{
					RandomBilateral(series) * 0.5f * width,
					RandomBilateral(series) * 0.5f * height,
					0
				};
				bool grass = RandomUnilateral(series) > 0.5f;
				BitmapId bid = grass ?
					GetRandomBitmapId(*args->assets, Asset_Grass, series) :
					GetRandomBitmapId(*args->assets, Asset_Ground, series);
				position.X += chunkOffsetX * width;
				position.Y += chunkOffsetY * height;
				PushBitmap(group, *args->assets, bid, position, 1.7f, V2{ 0, 0 }, color);
			}
		}
	}
	RenderGroupToBuffer(group, *buffer);
	WriteCompilatorFence;
	args->groundBuffer->state = GroundBufferState::Ready;
	EndBackgroundTask(task);
}

internal
bool FillGroundBuffer(TransientState* tranState, ProgramState* state, GroundBuffer& dstBuffer, WorldPosition& chunkPos, PlatformQueue* queue) {
	if (!LoadIfNotAllAssetsAreReady(tranState->assets, Asset_Ground) ||
		!LoadIfNotAllAssetsAreReady(tranState->assets, Asset_Grass)) {
		return false;
	}
	TaskWithMemory* task = TryBeginBackgroundTask(tranState);
	if (!task || dstBuffer.state == GroundBufferState::Pending) {
		return false;
	}
	FillGroundBufferTaskArgs* args = PushStructSize(task->arena, FillGroundBufferTaskArgs);
	args->chunkPos = chunkPos;
	args->chunkSizeInMeters = state->world.chunkSizeInMeters.XY;
	args->assets = &tranState->assets;
	args->groundBuffer = &dstBuffer;
	args->task = task;
	dstBuffer.pos = chunkPos;
	dstBuffer.state = GroundBufferState::Pending;
	WriteCompilatorFence;
	Platform->QueuePushTask(queue, FillGroundBufferBackgroundTask, args);
	return true;
}

inline
V2 ToBottomUpAlignment(LoadedBitmap& bitmap, V2 topDownAlign) {
	V2 result = V2{ topDownAlign.X, f4(bitmap.height - 1) - topDownAlign.Y };
	return result;
}

CollisionVolumeGroup* MakeGroundedCollisionGroup(ProgramState* state, V3 size) {
	CollisionVolumeGroup* group = PushStructSize(state->world.arena, CollisionVolumeGroup);
	group->volumeCount = 1;
	group->volumes = PushArray(state->world.arena, group->volumeCount, CollisionVolume);
	group->volumes[0].size = size;
	group->totalVolume = group->volumes[0];
	return group;
}

inline
void InitializeHitPoints(Entity& entity, u32 nHitPoints, u32 amount, u32 max) {
	for (u32 hitPointIndex = 0; hitPointIndex < nHitPoints; hitPointIndex++) {
		Assert(hitPointIndex < ArrayCount(entity.hitPoints.hitPoints));
		entity.hitPoints.hitPoints[hitPointIndex].amount = amount;
		entity.hitPoints.hitPoints[hitPointIndex].max = max;
	}
	entity.hitPoints.count = nHitPoints;
}

internal
u32 AddGroundedEntity(World& world, EntityStorage& storage, u32 absX, u32 absY, u32 absZ,
	CollisionVolumeGroup* collision) {
	V3 offset = V3{ 0, 0, 0.5f * collision->totalVolume.size.Z };
	storage.entity.worldPos = GetChunkPositionFromWorldPosition(world, absX, absY, absZ, offset);
	storage.entity.collision = collision;
	return AddEntity(world, storage);
}


internal
u32 AddWall(ProgramState* state, World& world, u32 absX, u32 absY, u32 absZ) {
	EntityStorage storage = {};
	SetFlag(storage.entity, EntityFlag_StopsOnCollide);
	storage.entity.type = EntityType_Wall;
	return AddGroundedEntity(world, storage, absX, absY, absZ, state->wallCollision);
}

internal
u32 AddSpace(ProgramState* state, World& world, u32 centerX, u32 centerY, u32 minZ, V3 size) {
	EntityStorage storage = {};
	SetFlag(storage.entity, EntityFlag_Traversable);
	storage.entity.type = EntityType_Space;
	storage.entity.worldPos = GetChunkPositionFromWorldPosition(world, centerX, centerY, minZ, V3{ 0, 0, 0.5f * size.Z });
	storage.entity.collision = MakeGroundedCollisionGroup(state, size);
	return AddEntity(world, storage);
}


internal
u32 AddStairs(ProgramState* state, World& world, u32 absX, u32 absY, u32 absZ) {
	EntityStorage storage = {};
	storage.entity.walkableDim = V3{
		state->stairsCollision->totalVolume.size.X,
		state->stairsCollision->totalVolume.size.Y,
		world.tileSizeInMeters.Z
	};
	SetFlag(storage.entity, EntityFlag_Overlaps);
	storage.entity.type = EntityType_Stairs;
	return AddGroundedEntity(world, storage, absX, absY, absZ, state->stairsCollision);
}

internal
u32 AddFamiliar(ProgramState* state, World& world, u32 absX, u32 absY, u32 absZ) {
	EntityStorage storage = {};
	storage.entity.worldPos = GetChunkPositionFromWorldPosition(world, absX, absY, absZ);
	storage.entity.type = EntityType_Familiar;
	storage.entity.collision = state->familiarCollision;
	SetFlag(storage.entity, EntityFlag_Movable);
	return AddEntity(world, storage);
}

internal
u32 AddSword(ProgramState* state, World& world) {
	EntityStorage storage = {};
	storage.entity.worldPos = NullPosition();
	storage.entity.collision = state->swordCollision;
	storage.entity.type = EntityType_Sword;
	SetFlag(storage.entity, EntityFlag_Movable);
	return AddEntity(world, storage);
}

internal
u32 AddMonster(ProgramState* state, World& world, u32 absX, u32 absY, u32 absZ) {
	EntityStorage storage = {};
	SetFlag(storage.entity, EntityFlag_StopsOnCollide | EntityFlag_Movable);
	storage.entity.type = EntityType_Monster;
	InitializeHitPoints(storage.entity, 3, 1, 1);
	return AddGroundedEntity(world, storage, absX, absY, absZ, state->monsterCollision);
}

internal
u32 InitializePlayer(ProgramState* state) {
	EntityStorage storage = {};
	storage.entity.faceDir = 0;
	storage.entity.type = EntityType_Player;
	SetFlag(storage.entity, EntityFlag_StopsOnCollide | EntityFlag_Movable);
	InitializeHitPoints(storage.entity, 4, 1, 1);
	u32 swIndex = AddSword(state, state->world);
	EntityStorage* swordStorage = GetEntityStorage(state->world, swIndex);
	swordStorage->entity.storageIndex = swIndex;
	storage.entity.sword = &swordStorage->entity;
	Assert(storage.entity.sword);
	u32 index = AddGroundedEntity(state->world, storage, 8, 5, 0, state->playerCollision);
	Assert(index);
	if (!state->cameraEntityIndex) {
		state->cameraEntityIndex = index;
	}
	return index;
}

struct TestWall {
	f32 maxX;
	f32 maxY;
	f32 minY;
	f32 deltaX;
	f32 deltaY;
	V3 normal;
};

bool TestForTMinCollision(f32 maxCornerX, f32 maxCornerY, f32 minCornerY, f32 moveDeltaX,
					  f32 moveDeltaY, f32* tMin) {
	if (moveDeltaX != 0) {
		f32 tEpsilon = 0.001f;
		f32 tCollision = maxCornerX / moveDeltaX;
		if (tCollision >= 0.f && tCollision < *tMin) {
			f32 Y = moveDeltaY * tCollision;
			if (Y >= minCornerY && Y < maxCornerY) {
				*tMin = Maximum(0.f, tCollision - tEpsilon);
				return true;
			}
		}
	}
	return false;
}

bool TestForTMaxCollision(f32 maxCornerX, f32 maxCornerY, f32 minCornerY, f32 moveDeltaX,
	f32 moveDeltaY, f32* tMax) {
	if (moveDeltaX != 0) {
		f32 tEpsilon = 0.001f;
		f32 tCollision = maxCornerX / moveDeltaX;
		if (tCollision >= 0.f && tCollision > *tMax) {
			f32 Y = moveDeltaY * tCollision;
			if (Y >= minCornerY && Y < maxCornerY) {
				*tMax = tCollision - tEpsilon;
				return true;
			}
		}
	}
	return false;
}

inline
u32 GetCollisionHashValue(World& world, u32 hashEntry) {
	Assert(hashEntry);
	// TODO: Better hash function
	u32 hash = hashEntry & (ArrayCount(world.hashCollisions) - 1);
	return hash;
}

internal
void AddCollisionRule(World& world, MemoryArena& arena, u32 firstStorageIndex, u32 secondStorageIndex) {
	// TODO hold multiple indexes inside one block instead of pair per block cause this is memory inefficient
	u32 hash = GetCollisionHashValue(world, firstStorageIndex);
	PairwiseCollision* block = world.hashCollisions[hash];
	PairwiseCollision* newPair = 0;
	if (world.freeCollisionsList) {
		newPair = world.freeCollisionsList;
		world.freeCollisionsList = world.freeCollisionsList->next;
	}
	else {
		newPair = PushStructSize(arena, PairwiseCollision);
	}
	newPair->storageIndex1 = firstStorageIndex;
	newPair->storageIndex2 = secondStorageIndex;
	world.hashCollisions[hash] = newPair;
	newPair->next = block;
}

internal
void ClearCollisionRuleForEntityPair(World& world, u32 firstStorageIndex, u32 secondStorageIndex) {
	u32 hash = GetCollisionHashValue(world, firstStorageIndex);
	PairwiseCollision* firstBlock = world.hashCollisions[hash];
	if (!firstBlock) {
		Assert(!"Tried to clear non existing rule. Invalid code path");
		return;
	}
	PairwiseCollision* previousBlock = 0;
	for (PairwiseCollision* block = firstBlock; block; block = block->next) {
		if (!block) {
			Assert(!"Tried to clear non existing rule. Invalid code path");
			break;
		}
		if (block->storageIndex1 == firstStorageIndex &&
			block->storageIndex2 == secondStorageIndex) {
			if (previousBlock) {
				previousBlock->next = block->next;
			}
			else {
				world.hashCollisions[hash] = block->next;
			}
			block->storageIndex1 = 0;
			block->storageIndex2 = 0;
			block->next = world.freeCollisionsList;
			world.freeCollisionsList = block;
			break;
		}
		previousBlock = block;
	}
}

internal
void ClearCollisionRuleForEntity(World& world, u32 entityStorageIndex) {
	u32 hash = GetCollisionHashValue(world, entityStorageIndex);
	PairwiseCollision* firstBlock = world.hashCollisions[hash];
	if (!firstBlock) {
		return;
	}
	PairwiseCollision* previousBlock = 0;
	PairwiseCollision* block = firstBlock;
	while (block) {
		if (block->storageIndex1 == entityStorageIndex) {
			u32 secondStorageIndex = block->storageIndex2;
			ClearCollisionRuleForEntityPair(world, secondStorageIndex, entityStorageIndex);	
			if (previousBlock) {
				previousBlock->next = block->next;
			}
			else {
				world.hashCollisions[hash] = block->next;
			}
			PairwiseCollision* tmp = block->next;
			block->storageIndex1 = 0;
			block->storageIndex2 = 0;
			block->next = world.freeCollisionsList;
			world.freeCollisionsList = block;
			block = tmp;
		}
		else {
			previousBlock = block;
			block = block->next;
		}
	}
}


internal
bool ShouldCollide(World& world, u32 firstStorageIndex, u32 secondStorageIndex) {
	static_assert((ArrayCount(world.hashCollisions) & (ArrayCount(world.hashCollisions) - 1)) == 0 &&
		"hashValue is ANDed with a mask based with assert that the size of hashCollisions is power of two");
	Assert(firstStorageIndex);
	Assert(secondStorageIndex);
	Assert(firstStorageIndex != secondStorageIndex);

	u32 hash = GetCollisionHashValue(world, firstStorageIndex);
	PairwiseCollision* firstBlock = world.hashCollisions[hash];
	for (PairwiseCollision* block = firstBlock; block; block = block->next) {
		if (!block) {
			return true;
		}
		if (block->storageIndex1 == firstStorageIndex &&
			block->storageIndex2 == secondStorageIndex) {
			return false;
		}
	}
	return true;
}

inline
V3 GetEntityGroundLevel(Entity& entity) {
	V3 result = entity.pos - V3{ 0, 0, 0.5f * entity.collision->totalVolume.size.Z };
	return result;
}

inline
void SetEntityGroundLevel(Entity& entity, f32 newGroundLevel) {
	entity.pos.Z = newGroundLevel + 0.5f * entity.collision->totalVolume.size.Z;
}

internal
void HandleOverlap(World& world, Entity& mover, Entity& obstacle, f32* ground) {
	if (mover.type == EntityType_Player && obstacle.type == EntityType_Stairs) {
		Assert(obstacle.type == EntityType_Stairs);
		V3 normalizedPos = PointRelativeToRect(
			GetRectFromCenterDim(obstacle.pos, obstacle.walkableDim), 
			mover.pos
		);
		f32 stairsLowerPosZ = GetEntityGroundLevel(obstacle).Z;
		f32 stairsUpperPosZ = stairsLowerPosZ + obstacle.walkableDim.Z;
		*ground = stairsLowerPosZ + normalizedPos.Y * obstacle.walkableDim.Z;
		//*ground = Clip(*ground, stairsLowerPosZ, stairsUpperPosZ);
	}
}

internal 
bool EntitiesOverlaps(Entity& entity, Entity& other) {
	for (u32 entityVolumeIndex = 0;
		entityVolumeIndex < entity.collision->volumeCount;
		entityVolumeIndex++) {
		CollisionVolume* entityVolume = entity.collision->volumes + entityVolumeIndex;
		for (u32 obstacleVolumeIndex = 0;
			obstacleVolumeIndex < other.collision->volumeCount;
			obstacleVolumeIndex++) {
			CollisionVolume* obstacleVolume = other.collision->volumes + obstacleVolumeIndex;
			if (EntityOverlapsWithRegion(entity.pos + entityVolume->offsetPos, entityVolume->size,
				GetRectFromCenterDim(other.pos + obstacleVolume->offsetPos, obstacleVolume->size)
			)) {
				return true;
			}
		}
	}
	return false;
}

internal
bool HandleCollision(World& world, Entity& mover, Entity& obstacle) {
	// TODO: Think of better approach of collision handling. This is prototype
	bool stopOnCollide = (
		(IsFlagSet(mover, EntityFlag_StopsOnCollide) && 
		IsFlagSet(obstacle, EntityFlag_StopsOnCollide)) ||
		IsFlagSet(obstacle, EntityFlag_Traversable)
	);

	if (mover.type == EntityType_Familiar && obstacle.type == EntityType_Wall) {
		stopOnCollide = true;
	}
	if (!ShouldCollide(world, mover.storageIndex, obstacle.storageIndex)) {
		return stopOnCollide;
	}
	if (mover.type == EntityType_Sword && obstacle.type == EntityType_Monster) {
		if (obstacle.hitPoints.count > 0) {
			obstacle.hitPoints.count--;
		}
		AddCollisionRule(world, world.arena, mover.storageIndex, obstacle.storageIndex);
		AddCollisionRule(world, world.arena, obstacle.storageIndex, mover.storageIndex);
	}
	if (mover.type == EntityType_Player && obstacle.type == EntityType_Stairs) {
		V3 normMoverPos = PointRelativeToRect(
			GetRectFromCenterDim(obstacle.pos, obstacle.walkableDim), 
			mover.pos
		);
		if (normMoverPos.Z < 0.5f && normMoverPos.Y < 0.1f ||
			normMoverPos.Z >= 0.5f && normMoverPos.Y >= 0.9f) {
			stopOnCollide = false;
		}
		else {
			stopOnCollide = true;
		}
	}
	return stopOnCollide;
}

//void QuickSortHitEntitiesByAscendingT(HitEntity* array, u32 low, u32 high) {
//	if (low >= high || low < 0) {
//		return;
//	}
//
//	u32 pivotIndex = low;
//	HitEntity pivot = array[high];
//	for (u32 index = low; index < high - 1; index++) {
//		if (array[index].t <= pivot.t) {
//			HitEntity tmp = array[index];
//			array[index] = array[pivotIndex];
//			array[pivotIndex] = array[index];
//			pivotIndex = low + 1;
//		}
//	}
//	HitEntity tmp = array[pivotIndex];
//	array[pivotIndex] = array[high];
//	array[high] = array[pivotIndex];
//
//	QuickSortHitEntitiesByAscendingT(array, low, pivotIndex - 1);
//	QuickSortHitEntitiesByAscendingT(array, pivotIndex + 1, high);
//}

internal
void MoveEntity(SimRegion& simRegion, ProgramState* state, World& world, Entity& entity, V3 acceleration, f32 dt) {
	f32 distanceRemaining = (entity.distanceRemaining != 0.f) ?
		entity.distanceRemaining :
		100000.f;
	V3 moveDelta = 0.5f * acceleration * Squared(dt) + entity.vel * dt;
	f32 moveDeltaLength = Length(moveDelta);
	if (moveDeltaLength > distanceRemaining) {
		moveDelta *= distanceRemaining / moveDeltaLength;
	}

	entity.vel += acceleration * dt;
	V3 nextPlayerPosition = entity.pos + moveDelta;

	// TODO: G.J.K algorithm for other collision shapes like circles, elipses etc.
	for (u32 iteration = 0; iteration < 4; iteration++) {
		f32 tMin = 1.0f;
		f32 tMax = 0.0f;
		V3 wallNormalMin = {};
		V3 wallNormalMax = {};
		Entity* hitEntityMin = 0;
		Entity* hitEntityMax = 0;
		V3 desiredPosition = entity.pos + moveDelta;
		for (u32 entityIndex = 0; entityIndex < simRegion.entityCount; entityIndex++) {
			Entity* other = simRegion.entities + entityIndex;
			if (!other || IsFlagSet(*other, EntityFlag_NonSpatial) || other->storageIndex == entity.storageIndex) {
				continue;
			}
			if (IsFlagSet(*other, EntityFlag_Traversable)) {
				if (!EntitiesOverlaps(entity, *other)) {
					continue;
				}
				for (u32 entityVolumeIndex = 0; entityVolumeIndex < entity.collision->volumeCount; entityVolumeIndex++) {
					CollisionVolume* entityVolume = entity.collision->volumes + entityVolumeIndex;
					for (u32 obstacleVolumeIndex = 0; obstacleVolumeIndex < other->collision->volumeCount; obstacleVolumeIndex++) {
						CollisionVolume* obstacleVolume = other->collision->volumes + obstacleVolumeIndex;

						V3 diff = other->pos + obstacleVolume->offsetPos - entity.pos - entityVolume->offsetPos;
						V3 minCorner = diff - 0.5f * obstacleVolume->size - 0.5f * entityVolume->size;
						V3 maxCorner = diff + 0.5f * obstacleVolume->size + 0.5f * entityVolume->size;

						// TODO: Handle Z axis in collisions more properly
						if (entity.pos.Z + entityVolume->offsetPos.Z >= maxCorner.Z ||
							entity.pos.Z + entityVolume->offsetPos.Z < minCorner.Z) {
							continue;
						}
						TestWall testWalls[] = {
							TestWall{ maxCorner.X, maxCorner.Y, minCorner.Y, moveDelta.X, moveDelta.Y, V3{ -1.f, 0.f, 0.f } },
							TestWall{ minCorner.X, maxCorner.Y, minCorner.Y, moveDelta.X, moveDelta.Y, V3{ 1.f, 0.f, 0.f } },
							TestWall{ maxCorner.Y, maxCorner.X, minCorner.X, moveDelta.Y, moveDelta.X, V3{ 0.f, -1.f, 0.f } },
							TestWall{ minCorner.Y, maxCorner.X, minCorner.X, moveDelta.Y, moveDelta.X, V3{ 0.f, 1.f, 0.f }}
						};
						// TODO: When hit test is true for specific entity pair: 
						// we can break from volume/volume O^2 loop as a optimization
						if (entity.type == EntityType_Player) {
							int breakHere = 5;
						}
						for (u32 wallIndex = 0; wallIndex < ArrayCount(testWalls); wallIndex++) {
							TestWall* wall = testWalls + wallIndex;
							if (TestForTMaxCollision(wall->maxX, wall->maxY, wall->minY, wall->deltaX,
								wall->deltaY, &tMax)) {
								// Right wall
								wallNormalMax = wall->normal;
								hitEntityMax = other;
							}
						}
					}
				}
			}
			else {
				for (u32 entityVolumeIndex = 0; entityVolumeIndex < entity.collision->volumeCount; entityVolumeIndex++) {
					CollisionVolume* entityVolume = entity.collision->volumes + entityVolumeIndex;
					for (u32 obstacleVolumeIndex = 0; obstacleVolumeIndex < other->collision->volumeCount; obstacleVolumeIndex++) {
						CollisionVolume* obstacleVolume = other->collision->volumes + obstacleVolumeIndex;

						V3 diff = other->pos + obstacleVolume->offsetPos - entity.pos - entityVolume->offsetPos;
						V3 minCorner = diff - 0.5f * obstacleVolume->size - 0.5f * entityVolume->size;
						V3 maxCorner = diff + 0.5f * obstacleVolume->size + 0.5f * entityVolume->size;

						// TODO: Handle Z axis in collisions more properly
						if (entity.pos.Z + entityVolume->offsetPos.Z >= maxCorner.Z ||
							entity.pos.Z + entityVolume->offsetPos.Z < minCorner.Z) {
							continue;
						}
						TestWall testWalls[] = {
							TestWall{ maxCorner.X, maxCorner.Y, minCorner.Y, moveDelta.X, moveDelta.Y, V3{ 1.f, 0.f, 0.f } },
							TestWall{ minCorner.X, maxCorner.Y, minCorner.Y, moveDelta.X, moveDelta.Y, V3{ -1.f, 0.f, 0.f } },
							TestWall{ maxCorner.Y, maxCorner.X, minCorner.X, moveDelta.Y, moveDelta.X, V3{ 0.f, 1.f, 0.f } },
							TestWall{ minCorner.Y, maxCorner.X, minCorner.X, moveDelta.Y, moveDelta.X, V3{ 0.f, -1.f, 0.f }}
						};
						// TODO: When hit test is true for specific entity pair: 
						// we can break from volume/volume O^2 loop as a optimization
						for (u32 wallIndex = 0; wallIndex < ArrayCount(testWalls); wallIndex++) {
							TestWall* wall = testWalls + wallIndex;
							if (TestForTMinCollision(wall->maxX, wall->maxY, wall->minY, wall->deltaX,
								wall->deltaY, &tMin)) {
								// Right wall
								wallNormalMin = wall->normal;
								hitEntityMin = other;
							}
						}
					}
				}
			}
		}
		if (entity.type == EntityType_Player) {
			int breakHere = 5;
		}
		f32 tMove;
		Entity* hitEntity;
		V3 wallNormal;
		if (tMin < tMax) {
			tMove = tMin;
			hitEntity = hitEntityMin;
			wallNormal = wallNormalMin;
		}
		else {
			tMove = tMax;
			hitEntity = hitEntityMax;
			wallNormal = wallNormalMax;
		}		
		entity.pos += moveDelta * tMove;
		if (entity.distanceRemaining != 0.f) {
			constexpr f32 dEpsilon = 0.01f;
			entity.distanceRemaining -= Length(moveDelta * tMove);
			if (entity.distanceRemaining < dEpsilon) {
				entity.distanceRemaining = 0.f;
			}
		}
		if (hitEntity) {
			bool stopOnCollision = HandleCollision(world, entity, *hitEntity);
			if (stopOnCollision) {
				entity.vel -= Inner(entity.vel, wallNormal) * wallNormal;
				moveDelta = desiredPosition - entity.pos;
				moveDelta -= Inner(moveDelta, wallNormal) * wallNormal;
			}
			else {
				// TODO: Can it be done in a better, smarter way?
				// TODO: distance should be updated here as well
				moveDelta = desiredPosition - entity.pos;
				entity.pos += moveDelta * (1.f - tMove);
				break;
			}
		}
		if (tMove == 1.0f) {
			break;
		}
	}

	u32 overlapEntityCount = 0;
	u32 overlapEntities[16] = {};
	// Note: Even if simulation won't be as accurate, because between hit iterations
	// entity could potentially overlap with another entity, having this after all
	// hit iterations seems to be good aproximation cause we don't need to mess up
	// with lots of stuff. It's worth noting that in case of large dt it can result
	// in missing some overlaps 
	// TODO: (maybe it should be done more precisely for simulation with larger dt)
	for (u32 entityIndex = 0; entityIndex < simRegion.entityCount; entityIndex++) {
		Entity* other = simRegion.entities + entityIndex;
		if (!other || IsFlagSet(*other, EntityFlag_NonSpatial) ||
			other->storageIndex == entity.storageIndex) {
			continue;
		}
		if (EntitiesOverlaps(entity, *other)) {
			Assert(overlapEntityCount < ArrayCount(overlapEntities));
			if (overlapEntityCount < ArrayCount(overlapEntities)) {
				overlapEntities[overlapEntityCount++] = entityIndex;
			}
		}
	}

	f32 ground = simRegion.distanceToClosestGroundZ;
	for (u32 overlapEntityIdx = 0; overlapEntityIdx < overlapEntityCount; overlapEntityIdx++) {
		Entity* other = simRegion.entities + overlapEntities[overlapEntityIdx];
		HandleOverlap(world, entity, *other, &ground);
	}
	f32 entityGroundLevel = GetEntityGroundLevel(entity).Z;
	if (entityGroundLevel != ground) {
		if (entity.type == EntityType_Player) {
			int breakHere = 5;
		}
		SetEntityGroundLevel(entity, ground);
		entity.vel.Z = 0.f;
	}
	f32 velEpsilon = 0.5f;
	if (entity.vel.X < -velEpsilon ||
		entity.vel.X > velEpsilon ||
		entity.vel.Y < -velEpsilon ||
		entity.vel.Y > velEpsilon) {
		entity.faceDir = Atan2(entity.vel.Y, entity.vel.X);
	}
}

internal
LoadedBitmap MakeEmptyBuffer(MemoryArena& arena, u32 width, u32 height) {
	LoadedBitmap bmp = {};
	u64 alignment = 32;
	bmp.data = PushArray(arena, width * height, u32, alignment);
	bmp.bufferStart = ptrcast(void, bmp.data);
	bmp.height = height;
	bmp.width = width;
	bmp.widthOverHeight = f4(width) / f4(height);
	bmp.pitch = width * BITMAP_BYTES_PER_PIXEL;
	return bmp;
}

internal
void SetCamera(ProgramState* state) {
	EntityStorage* cameraEntityStorage = GetEntityStorage(state->world, state->cameraEntityIndex);
	if (cameraEntityStorage) {
		state->cameraPos = OffsetWorldPosition(state->world, state->cameraPos, cameraEntityStorage->entity.pos);
	}
}

internal
void MakeEntitySpatial(SimRegion& simRegion, World& world, u32 storageEntityIndex, Entity& entity, WorldPosition& newPos) {
	Assert(IsFlagSet(entity, EntityFlag_NonSpatial));
	ChangeEntityChunkLocation(world, world.arena, storageEntityIndex, entity, 0, newPos);
}

internal
void MakeEntityNonSpatial(ProgramState* state, u32 storageEntityIndex, Entity& entity) {
	Assert(!IsFlagSet(entity, EntityFlag_NonSpatial));
	SetFlag(entity, EntityFlag_NonSpatial);
}

LoadedBitmap MakeSphereNormalMap(MemoryArena& arena, u32 width, u32 height, f32 roughness) {
	LoadedBitmap result = MakeEmptyBuffer(arena, width, height);
	u8* row = ptrcast(u8, result.data);
	for (u32 Y = 0; Y < height; Y++) {	
		u32* pixel = ptrcast(u32, row);
		for (u32 X = 0; X < width; X++) {
			f32 U = f4(X) / f4(width - 1);
			f32 V = f4(Y) / f4(height - 1);
			Assert(U >= 0.f && U <= 1.f);
			Assert(V >= 0.f && V <= 1.f);
			U = 2 * U - 1;
			V = 2 * V - 1;
			f32 sqSumUV = Squared(U) + Squared(V);
			V4 normal;
			if (sqSumUV < 1.f) {
				f32 Z = SquareRoot(1 - sqSumUV);
				normal = V4{ U, V, Z, roughness };
			}
			else if(V > 0.f) {
				normal = V4{ U, V, (U + V) / 2.f, roughness };
			}
			else {
				normal = V4{ U, -V, (U + V) / 2.f, roughness };
			}
			normal.XYZ = Normalize(normal.XYZ);
			*pixel = (u4(255.f * normal.W + 0.5f) << 24) |
					 (u4(127.5f * normal.X + 0.5f + 127.f) << 16) |
					 (u4(127.5f * normal.Y + 0.5f + 127.f) << 8) |
					 (u4(127.5f * normal.Z + 0.5f + 127.f) << 0);
			pixel++;
		}
		row += result.pitch;
	}
	return result;
}
LoadedBitmap MakeSphereDiffusionTexture(MemoryArena& arena, u32 width, u32 height) {
	LoadedBitmap result = MakeEmptyBuffer(arena, width, height);
	u8* row = ptrcast(u8, result.data);
	V4 color = { 0, 0, 0, 1 };
	for (u32 Y = 0; Y < height; Y++) {
		u32* pixel = ptrcast(u32, row);
		for (u32 X = 0; X < width; X++) {
			f32 alpha = 255.f;
			f32 U = f4(X) / f4(width - 1);
			f32 V = f4(Y) / f4(height - 1);
			Assert(U >= 0.f && U <= 1.f);
			Assert(V >= 0.f && V <= 1.f);
			f32 USq = Squared(f4(2 * U - 1));
			f32 VSq = Squared(f4(2 * V - 1));
			if (USq + VSq > 1.f) {
				alpha = 0.f;
			}
			alpha *= color.A;
			*pixel = (u4(alpha) << 24) |
					(u4(alpha * color.R) << 16) |
					(u4(alpha * color.G) << 8) |
					(u4(alpha * color.B) << 0);
			pixel++;
		}
		row += result.pitch;
	}
	return result;
}

PlayingSound* PlaySound(AudioState& audio, Assets& assets, SoundId soundId, f32 secondsToStart) {
	PlayingSound* playingSound = audio.freeListSounds;
	if (playingSound) {
		audio.freeListSounds = playingSound->next;
		playingSound->next = 0;
	}
	else {
		playingSound = PushStructSize(audio.arena, PlayingSound);
	}
	playingSound->currentSample = -secondsToStart * SOUND_SAMPLES_PER_SECOND;
	playingSound->next = 0;
	playingSound->soundId = soundId;
	playingSound->next = audio.playingSounds;
	playingSound->currentVolume = { 1.f, 1.f };
	playingSound->requestedVolume = playingSound->currentVolume;
	playingSound->volumeChangeSpeed = { 0.f, 0.f };
	playingSound->pitch = 1.0f;
	audio.playingSounds = playingSound;
	PrefetchSound(assets, soundId);
	return playingSound;
}

void ChangeVolume(PlayingSound* sound, V2 volume, f32 durationInSeconds) {
	if (!sound) {
		return;
	}
	sound->requestedVolume = volume;
	sound->volumeChangeSpeed = (sound->requestedVolume - sound->currentVolume) / 
		(durationInSeconds * SOUND_SAMPLES_PER_SECOND);
}

void ChangePitch(PlayingSound* sound, f32 pitch) {
	if (!sound) {
		return;
	}
	sound->pitch = pitch;
}

extern "C" GAME_MAIN_LOOP_FRAME(GameMainLoopFrame) {
	debugGlobalMemory = &memory.debug;
	Platform = &memory.platformAPI;
	BEGIN_TIMED_SECTION(GameMainLoop);
	ProgramState* state = ptrcast(ProgramState, memory.permanentMemory);
	World& world = state->world;
	if (!state->isInitialized) {
		InitializeWorld(world);
		InitializeArena(
			state->mainArena,
			ptrcast(u8, memory.permanentMemory) + sizeof(ProgramState),
			memory.permanentMemorySize - sizeof(ProgramState)
		);
		SubArena(world.arena, state->mainArena, MB(32));
		SubArena(state->audio.arena, state->mainArena, MB(16));

		state->wallCollision = MakeGroundedCollisionGroup(state, world.tileSizeInMeters);
		state->playerCollision = MakeGroundedCollisionGroup(state, V3{1.0f, 0.55f, 0.25f});
		state->monsterCollision = MakeGroundedCollisionGroup(state, V3{ 1.0f, 1.25f, 0.25f });
		state->familiarCollision = MakeGroundedCollisionGroup(state, V3{ 1.0f, 1.25f, 0.25f });
		state->swordCollision = MakeGroundedCollisionGroup(state, V3{ 0.5f, 0.5f, 0.25f });
		state->stairsCollision = MakeGroundedCollisionGroup(
			state,
			V3{ world.tileSizeInMeters.X, 
				2.0f * world.tileSizeInMeters.Y, 
				1.1f * world.tileSizeInMeters.Z }
		);

		state->highFreqBoundDim = 30.f;
		state->highFreqBoundHeight = 1.2f;
		state->cameraPos = GetChunkPositionFromWorldPosition(
			world, world.tileCountX / 2, world.tileCountY / 2, 0
		);

		u32 testTextureWidth = 256;
		u32 testTextureHeight = 256;
		state->testNormalMap = MakeSphereNormalMap(state->world.arena, testTextureWidth, testTextureHeight, 1.f);
		state->testDiffusionTexture = MakeSphereDiffusionTexture(state->world.arena, testTextureWidth, testTextureHeight);

		bool doorLeft = false;
		bool doorRight = false;
		bool doorUp = false;
		bool doorDown = false;
		bool lvlJustChanged = false;
		bool stairs = false;
		u32 screenX = 0;
		u32 screenY = 0;
		u32 absTileZ = 0;
		RandomSeries series = {};
#if 1
		for (u32 screenIndex = 0; screenIndex < 100; screenIndex++) {
#if 1
			u32 randomNumber = NextRandom(series);
#else
			u32 randomNumber = 2;
#endif
#if 1
			u32 mod = randomNumber % 3;
#else
			u32 mod = randomNumber % 2;
#endif
			if (mod == 0) {
				doorRight = true;
			}
			else if (mod == 1) {
				doorUp = true;
			}
			else if (mod == 2) {
				stairs = true;
			}

			u32 roomCenterX = screenX * world.tileCountX + world.tileCountX / 2;
			u32 roomCenterY = screenY * world.tileCountY + world.tileCountY / 2;
			V3 roomSize = V3{ world.tileCountX * world.tileSizeInMeters.X,
							  world.tileCountY * world.tileSizeInMeters.Y,
							  world.tileSizeInMeters.Z };
			AddSpace(state, world, roomCenterX, roomCenterY, absTileZ, roomSize);
			for (u32 tileY = 0; tileY < world.tileCountY; tileY++) {
				for (u32 tileX = 0; tileX < world.tileCountX; tileX++) {
					u32 absTileX = screenX * world.tileCountX + tileX;
					u32 absTileY = screenY * world.tileCountY + tileY;

					u32 tileValue = 1;
					bool putStairs = false;
					if (tileX == 0) {
						tileValue = 2;
						if (doorLeft && tileY == world.tileCountY / 2) {
							tileValue = 1;
						}
					} 
					else if (tileY == 0) {
						tileValue = 2;
						if (doorDown && tileX == world.tileCountX / 2) {
							tileValue = 1;
						}
					} 
					else if (tileX == world.tileCountX - 1) {
						tileValue = 2;
						if (doorRight && tileY == world.tileCountY / 2) {
							tileValue = 1;
						}
					}
					else if (tileY == world.tileCountY - 1) {
						tileValue = 2;
						if (doorUp && tileX == world.tileCountX / 2) {
							tileValue = 1;
						}
					}
					u32 stairPosX = 9;
					u32 stairPosY = 5;
					if (stairs) {
						if (absTileZ % 2 == 0 && tileX == stairPosX && tileY == stairPosY) {
							putStairs = true; // Ladder up
						}
						else if (absTileZ % 2 == 1 && tileX == stairPosX - 3 && tileY == stairPosY) {
							putStairs = true; // Ladder up
						}
					}
					
					if (tileValue == 2) {
						AddWall(state, world, absTileX, absTileY, absTileZ);
					}
					if (putStairs) {
						AddStairs(state, world, absTileX, absTileY, absTileZ);
					}				
				}
			}
			doorLeft = doorRight;
			doorDown = doorUp;
			doorUp = false;
			doorRight = false;

			if (mod == 0) {
				screenX++;
			}
			else if (mod == 1) {
				screenY++;
			}
			else if (mod == 2) {
				absTileZ++;
				stairs = false;
			}
		}
		AddFamiliar(state, world, 17 / 2, 9 / 2, 0);
		AddMonster(state, world, 17 / 2, 7, 0);
		AddWall(state, world, 17 / 2, 4, 0);
#endif
		state->isInitialized = true;
	}
	
	TransientState* tranState = ptrcast(TransientState, memory.transientMemory);
	if (!tranState->isInitialized) {
		InitializeArena(
			tranState->arena,
			ptrcast(u8, memory.transientMemory) + sizeof(TransientState),
			memory.transientMemorySize - sizeof(TransientState)
		);
		tranState->highPriorityQueue = memory.highPriorityQueue;
		tranState->lowPriorityQueue = memory.lowPriorityQueue;

		AllocateAssets(tranState);
		for (u32 taskIndex = 0; taskIndex < ArrayCount(tranState->tasks); taskIndex++) {
			TaskWithMemory* task = tranState->tasks + taskIndex;
			//Note: Hot reload causes crash, when background tasks are working
			SubArena(task->arena, tranState->arena, MB(4));
			task->done = 1;
		}
		PlaySound(state->audio, tranState->assets, GetFirstSoundIdWithType(tranState->assets, Asset_Music), 0);

		for (u32 groundBufferIndex = 0; groundBufferIndex < ArrayCount(tranState->groundBuffers); groundBufferIndex++) {
			GroundBuffer* groundBuffer = tranState->groundBuffers + groundBufferIndex;
			groundBuffer->buffer = MakeEmptyBuffer(tranState->arena, 256, 256);
			groundBuffer->pos = NullPosition();
			groundBuffer->state = GroundBufferState::NotReady;
		}
		tranState->isInitialized = true;
	}

	if (input.executableReloaded) {
		for (u32 groundBufferIndex = 0; groundBufferIndex < ArrayCount(tranState->groundBuffers); groundBufferIndex++) {
			GroundBuffer* groundBuffer = tranState->groundBuffers + groundBufferIndex;
			groundBuffer->pos = NullPosition();
		}
	}

	SetCamera(state);
	TemporaryMemory simMemory = BeginTempMemory(tranState->arena);
	ProjectionProps projection = GetStandardProjection(bitmap.width, bitmap.height);
	Rect2 cameraBounds = GetRenderRectangleAtTarget(projection, bitmap.width, bitmap.height);
	Rect3 simBounds = ToRect3(cameraBounds, V2{ -3, 1 } *state->world.tileSizeInMeters.Z);
	simBounds.max.X += 5.f;
	simBounds.min.X -= 5.f;
	simBounds.max.Y += 5.f;
	simBounds.min.Y -= 5.f;
	SimRegion* simRegion = BeginSimulation(*simMemory.arena, world, state->cameraPos, simBounds);
	for (u32 playerIdx = 0; playerIdx < MAX_CONTROLLERS; playerIdx++) {
		Controller& controller = input.controllers[playerIdx]; 
		PlayerControls& playerControls = state->playerControls[playerIdx];
		u32 playerLowEntityIndex = state->playerEntityIndexes[playerIdx];
		if (controller.B.kSpace.isDown && playerLowEntityIndex == 0) {
			playerLowEntityIndex = InitializePlayer(state);
			state->playerEntityIndexes[playerIdx] = playerLowEntityIndex;
		}

		Entity* entity = GetEntityByStorageIndex(*simRegion, playerLowEntityIndex);
		if (!entity) {
			continue;
		}
		playerControls.acceleration = {};
		f32 speed = 75.0f;
		if (controller.B.kA.isDown) {
			playerControls.acceleration.X -= 1.f;
			if (!controller.B.kA.wasDown) {
				PlayingSound* first = state->audio.playingSounds;
				ChangeVolume(first, V2{ 1.f, 0.f }, 5);
				PlaySound(state->audio, tranState->assets, GetFirstSoundIdWithType(tranState->assets, Asset_Bloop), 0);
			}
		}
		if (controller.B.kW.isDown) {
			playerControls.acceleration.Y += 1.f;
			if (!controller.B.kW.wasDown) {
				PlayingSound* first = state->audio.playingSounds;
				ChangeVolume(first, V2{ 1.f, 1.f }, 5);
			}
		}
		if (controller.B.kS.isDown) {
			playerControls.acceleration.Y -= 1.f;
			if (!controller.B.kS.wasDown) {
				PlayingSound* first = state->audio.playingSounds;
				ChangeVolume(first, V2{0.f, 0.f}, 5);
			}
		}
		if (controller.B.kD.isDown) {
			playerControls.acceleration.X += 1.f;
			if (!controller.B.kD.wasDown) {
				PlayingSound* first = state->audio.playingSounds;
				ChangeVolume(first, V2{ 0.f, 1.f }, 5);
			}
		}
		if (controller.B.kSpace.isDown) {
			speed = 250.0f;
			if (!controller.B.kSpace.wasDown) {
				PlayingSound* first = state->audio.playingSounds;
				static i32 index = -1;
				f32 pitches[] = { 0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.6f,
					0.7f, 0.8f, 0.9f, 1.0f, 1.1f, 1.2f, 1.3f, 1.4f,
					1.5f, 1.4f, 1.3f, 1.2f, 1.1f, 1.0f };
				index = (index + 1) % ArrayCount(pitches);
				ChangePitch(first, pitches[index]);
			}
		}
		if (controller.B.mouseLeft.isDown && IsFlagSet(*entity->sword, EntityFlag_NonSpatial)) {
			entity->sword->distanceRemaining = 5.f;
			entity->sword->timeRemaining = 4.f;

			V2 screenDim = GetDim(GetRenderRectangleAtTarget(projection, bitmap.width, bitmap.height));
			V2 mousePos = Hadamard(screenDim, controller.mouse - V2{0.5f, 0.5f});
			f32 mouseVecLength = Length(mousePos);
			f32 projectileSpeed = 5.f;
			if (mouseVecLength != 0.f) {
				entity->sword->vel.XY = mousePos / Length(mousePos) * projectileSpeed;
			}
			else {
				entity->sword->vel.XY = V2{ projectileSpeed, 0.f };
			}
			
			MakeEntitySpatial(*simRegion, state->world, entity->sword->storageIndex, *entity->sword, entity->worldPos);
		}
		PlayingSound* first = state->audio.playingSounds;
		f32 clampedMouseX = Clamp01(controller.mouse.X);
		ChangeVolume(first, V2{ 1 - clampedMouseX, clampedMouseX }, 0.1f);
		


		playerControls.acceleration.Z = 0.f;
		if (controller.B.kArrowUp.isDown) {
			playerControls.acceleration.Z += 10.0f;
		}
		if (controller.B.kArrowDown.isDown) {
			playerControls.acceleration.Z -= 10.0f;
		}
		f32 playerAccLength = Length(playerControls.acceleration);
		if (playerAccLength != 0) {
			playerControls.acceleration *= speed / Length(playerControls.acceleration);
		}
		playerControls.acceleration -= 10.0f * entity->vel;
	}
	LoadedBitmap screenBitmap = {};
	screenBitmap.height = bitmap.height;
	screenBitmap.width = bitmap.width;
	screenBitmap.data = ptrcast(u32, bitmap.data);
	screenBitmap.pitch = bitmap.pitch;

	// TODO: Think about size of the main render group
	TemporaryMemory renderMemory = BeginTempMemory(tranState->arena);
	RenderGroup renderGroup = AllocateRenderGroup(*renderMemory.arena, MB(4));
	renderGroup.projection = GetStandardProjection(bitmap.width, bitmap.height);
	f32 originalCameraDistance = renderGroup.projection.camera.distanceToTarget;
	//NOTE: Change this to change debug view
	//renderGroup.projection.camera.distanceToTarget = 30.f;

	Rect2 playerView = GetRenderRectangleAtDistance(renderGroup.projection, screenBitmap.width, screenBitmap.height, originalCameraDistance);
	PushClearCall(renderGroup, V4{ 0.2f, 0.2f, 0.2f, 1.f });

	Rect3 groundChunkBounds = ToRect3(playerView, V2{0, 0});
	WorldPosition minChunk = OffsetWorldPosition(world, state->cameraPos, GetMinCorner(groundChunkBounds));
	WorldPosition maxChunk = OffsetWorldPosition(world, state->cameraPos, GetMaxCorner(groundChunkBounds));
	for (i32 chunkY = minChunk.chunkY; chunkY <= maxChunk.chunkY; chunkY++) {
		for (i32 chunkX = minChunk.chunkX; chunkX <= maxChunk.chunkX; chunkX++) {
			i32 chunkZ = state->cameraPos.chunkZ;

			GroundBuffer* furthestBuffer = 0;
			f32 furthestDistanceSq = 0;
			GroundBuffer* drawBuffer = 0;
			for (u32 groundBufferIndex = 0; groundBufferIndex < ArrayCount(tranState->groundBuffers); groundBufferIndex++) {
				GroundBuffer* groundBuffer = tranState->groundBuffers + groundBufferIndex;
				if (groundBuffer->state == GroundBufferState::Pending) {
					continue;
				}
				
				if (groundBuffer->pos.chunkX == chunkX && 
					groundBuffer->pos.chunkY == chunkY &&
					groundBuffer->pos.chunkZ == chunkZ) 
				{
					drawBuffer = groundBuffer;
					break;
				}
				else if (IsValid(groundBuffer->pos)) {
					V3 diff = Subtract(world, groundBuffer->pos, state->cameraPos);
					f32 distance = LengthSq(diff);
					if (distance > furthestDistanceSq) {
						furthestDistanceSq = distance;
						furthestBuffer = groundBuffer;
					}
				}
				else {
					furthestDistanceSq = F32_MAX;
					furthestBuffer = groundBuffer;
				}
			}
			if (!drawBuffer && furthestBuffer) {
				WorldPosition chunkPos = CenteredWorldPosition(chunkX, chunkY, chunkZ);
				FillGroundBuffer(tranState, state, *furthestBuffer, chunkPos, tranState->lowPriorityQueue);
				drawBuffer = furthestBuffer;
			}
			if (drawBuffer->state == GroundBufferState::Ready) {
				V3 diff = Subtract(world, drawBuffer->pos, state->cameraPos);
				diff -= ToV3(0.5f * state->world.chunkSizeInMeters.XY, 0);
				PushBitmap(renderGroup, &drawBuffer->buffer, diff, 1.f * state->world.chunkSizeInMeters.Y, V2{ 0, 0 });
			}
		}
	}

#if 0
	for (i32 chunkY = minChunk.chunkY; chunkY <= maxChunk.chunkY; chunkY++) {
		for (i32 chunkX = minChunk.chunkX; chunkX <= maxChunk.chunkX; chunkX++) {
			i32 chunkZ = state->cameraPos.chunkZ;
			WorldPosition chunkPos = CenteredWorldPosition(chunkX, chunkY, chunkZ);
			V3 diff = Subtract(world, chunkPos, state->cameraPos);
			diff.Z = 0;
			PushRectBorders(renderGroup, diff, state->world.chunkSizeInMeters.XY, V4{1, 0, 0, 1}, 0.05f);
		}
	}
#endif
	PushRectBorders(renderGroup, V3{ 0.f, 0.f, 0.f }, GetDim(playerView), V4{ 1, 1, 0, 1 }, 0.4f);
	PushRectBorders(renderGroup, V3{ 0.f, 0.f, 0.f }, GetDim(simBounds).XY, V4{ 1, 0, 0, 1 }, 0.4f);
	f32 fadeUpStartZ = 0.f * state->world.tileSizeInMeters.Z;
	f32 fadeUpEndZ = 0.3f * state->world.tileSizeInMeters.Z;
	f32 fadeDownStartZ = -3.1f * state->world.tileSizeInMeters.Z;
	f32 fadeDownEndZ = -3.4f * state->world.tileSizeInMeters.Z;
	for (u32 entityIndex = 0; entityIndex < simRegion->entityCount; entityIndex++) {
		Entity* entity = simRegion->entities + entityIndex;
		if (!entity) {
			continue;
		}
		V3 acceleration = V3{ 0, 0, 0 };
		V3 groundLevelPos = GetEntityGroundLevel(*entity);
		f32 layerAlpha = 1.0f;
		if (groundLevelPos.Z > fadeUpStartZ) {
			f32 range = fadeUpEndZ - fadeUpStartZ;
			f32 Z = groundLevelPos.Z - fadeUpStartZ;
			layerAlpha = 1.0f - Clip01(Z / range);
		}
		else if (groundLevelPos.Z < fadeDownStartZ) {
			f32 range =  fadeDownEndZ - fadeDownStartZ;
			f32 Z = groundLevelPos.Z - fadeDownStartZ;
			layerAlpha = 1.0f - Clip01(Z / range);
		}
		switch(entity->type) {
		case EntityType_Player: {
			PlayerControls* playerControls = 0;
			for (u32 controllerIndex = 0; controllerIndex < MAX_CONTROLLERS; controllerIndex++) {
				u32 index = state->playerEntityIndexes[controllerIndex];
				if (index == entity->storageIndex) {
					playerControls = &state->playerControls[controllerIndex];
					break;
				}
			}
			Assert(playerControls);
			acceleration = playerControls->acceleration;
			const f32 playerHeight = 1.35f;
			PushRect(renderGroup, groundLevelPos, entity->collision->totalVolume.size.XY, V2{ 0, 0 }, V4{ 0, 1, 1, layerAlpha });
			AssetFeatures match = {};
			AssetFeatures weight = {};
			match[Feature_FacingDirection] = entity->faceDir;
			weight[Feature_FacingDirection] = 1.f;
			BitmapId bmp = GetBestFitBitmapId(tranState->assets, Asset_Player, match, weight, PI);
			PushBitmap(renderGroup, tranState->assets, bmp, groundLevelPos, 1.35f, V2{0, 0});
			RenderHitPoints(renderGroup, *entity, groundLevelPos, V2{0.f, -0.6f}, 0.1f, 0.2f, V4{ 1, 0, 0, layerAlpha });
		} break;
		case EntityType_Wall: {
			const f32 treeHeight = 2.5f * world.tileSizeInMeters.Z;
			PushRect(renderGroup, groundLevelPos, entity->collision->totalVolume.size.XY, V2{ 0, 0 }, V4{ 1, 1, 1, layerAlpha });
			AssetFeatures match = {};
			AssetFeatures weight = {};
			match[Feature_Height] = 1.5f;
			weight[Feature_Height] = 1.f;
			BitmapId bmp = GetBestFitBitmapId(tranState->assets, Asset_Tree, match, weight, 10000);
			PushBitmap(renderGroup, tranState->assets, bmp, groundLevelPos, treeHeight, 
				V2{0, 0.1f}, V4{1, 0.f, 1.f, layerAlpha});
		} break;
		case EntityType_Stairs: {
			PushRect(renderGroup, groundLevelPos, entity->collision->totalVolume.size.XY, V2{ 0, 0 }, V4{ 0.1f, 0.1f, 0.1f, layerAlpha });
			V3 upStairsPos = groundLevelPos + V3{ 0, 0, entity->walkableDim.Z };
			PushRect(renderGroup, upStairsPos, entity->collision->totalVolume.size.XY, V2{ 0, 0 }, V4{ 0, 0, 0, layerAlpha });
		} break;
		case EntityType_Familiar: {
			f32 minDistance = Squared(10.f);
			V3 minDistanceEntityPos = {};
			for (u32 otherEntityIndex = 0; otherEntityIndex < simRegion->entityCount; otherEntityIndex++) {
				Entity* other = simRegion->entities + otherEntityIndex;
				if (other->type == EntityType_Player) {
					f32 distance = LengthSq(other->pos - entity->pos);
					if (distance < minDistance) {
						minDistance = distance;
						minDistanceEntityPos = other->pos;
					}
				}
			}
			f32 speed = 50.0f;
			// TODO: delete this static float
			static float t = 0.f;
			if (minDistance > Squared(2.0f)) {
				acceleration = speed * (minDistanceEntityPos - entity->pos) / SquareRoot(minDistance);
			}
			acceleration.Z = 10.0f * Sin(6 * t);
			t += input.dtFrame;
			acceleration -= 10.0f * entity->vel;
			PushRect(renderGroup, groundLevelPos, entity->collision->totalVolume.size.XY,
				V2{ 0, 0 }, V4{ 0.f, 0.5f, 0.5f, layerAlpha });
		} break;
		case EntityType_Monster: {
			PushRect(renderGroup, groundLevelPos, entity->collision->totalVolume.size.XY, V2{ 0, 0 }, V4{ 1.f, 0.5f, 0, layerAlpha });
			RenderHitPoints(renderGroup, *entity, groundLevelPos, V2{ 0.f, -0.9f }, 0.1f, 0.2f, V4{ 1, 0, 0, layerAlpha });
		} break;
		case EntityType_Sword: {
			PushRect(renderGroup, groundLevelPos, entity->collision->totalVolume.size.XY, V2{0, 0}, V4{0, 0, 0, layerAlpha });
			entity->timeRemaining -= input.dtFrame;
			if (entity->distanceRemaining <= 0.f || entity->timeRemaining <= 0.f) {
				ClearCollisionRuleForEntity(state->world, entity->storageIndex);
				MakeEntityNonSpatial(state, entity->storageIndex, *entity);
			}
		} break;
		case EntityType_Space: {
			PushRectBorders(renderGroup, groundLevelPos, entity->collision->totalVolume.size.XY, V4{0, 0, 1, layerAlpha }, 0.2f);
		} break;
		default: Assert(!"Function to draw entity not found!");
		}
		if (IsFlagSet(*entity, EntityFlag_Movable) && !IsFlagSet(*entity, EntityFlag_NonSpatial)) {
			MoveEntity(*simRegion, state, world, *entity, acceleration, input.dtFrame);
		}
	}
#if 0
	EnvironmentMap* maps[3] = {
		&tranState->topEnvMap,
		&tranState->middleEnvMap,
		&tranState->bottomEnvMap,
	};
	V4 colors[3] = {
		V4{ 1, 0, 0, 1 },
		V4{ 0, 1, 0, 1 },
		V4{ 0, 1, 1, 1 }
	};
	u32 checkboardSize = 16;
	for (u32 mapIndex = 0; mapIndex < ArrayCount(maps); mapIndex++) {
		EnvironmentMap* map = maps[mapIndex];
		LoadedBitmap* LOD = map->LOD;
		*LOD = MakeEmptyBuffer(tranState->arena, 256, 128);

		
		u8* row = ptrcast(u8, LOD->data);
		V4 envMapColor = colors[mapIndex];
		f32 envMapColorIntensity = 1.f;
		f32 colorMultiplier = 1.0f;
		f32 whiteIntensity = 1.f - envMapColorIntensity;
		for (i32 Y = 0; Y < LOD->height; Y++) {
			u32* pixel = ptrcast(u32, row);
			if (((Y + 1) % 16) == 0) {
				envMapColorIntensity *= colorMultiplier;
				whiteIntensity = 1.f - envMapColorIntensity;
			}
			for (i32 X = 0; X < LOD->width; X++) {
				V4 pixColor;
				bool checkBoard = ((X / checkboardSize) % 2 == 0 &&
					(Y / checkboardSize) % 2 == 0) ||
					((X / checkboardSize) % 2 == 1 &&
						(Y / checkboardSize) % 2 == 1);
				if (checkBoard) {
					pixColor = envMapColor;
					pixColor.RGB *= envMapColorIntensity;
				}
				else {
					pixColor = V4{ 1, 1, 1, 1 };
					pixColor.RGB *= whiteIntensity;
				}
				f32 alpha = 255.f * pixColor.A;
				*pixel++ = (u4(alpha) << 24) |
					(u4(alpha * pixColor.R) << 16) |
					(u4(alpha * pixColor.G) << 8) |
					(u4(alpha * pixColor.B) << 0);
			}
			row += LOD->pitch;
		}
	}

	static f32 time = 0;
	time += input.dtFrame * 0.1f;
	f32 angle = time;
	V2 screenCenter = 0.5f * V2i(bitmap.width, bitmap.height);
	V2 origin = screenCenter;

	V2 xAxis = 256.f * V2{ 1.f, 0.f };
#define rotation 1
#define move_origin_x 1
#define move_origin_y 1
#define extend_x 0
#if rotation == 1
	f32 l = Length(xAxis);
	xAxis.X = l * Cos(3.5f * angle);
	xAxis.Y = l * Sin(3.5f * angle);
#endif
#if move_origin_x == 1
	origin += V2{ 100.f * Sin(10.f * angle), 0.f };
#endif
#if move_origin_y == 1
	origin += V2{ 0.f, 70.f * Cos(5.f * angle) };
#endif
	V2 yAxis = Perp(xAxis);
#if extend_x
	//yAxis *= 0.5f;
	xAxis *= 2.f;
#endif


	V4 color = V4{ 1.f, 1.f, 0.f, 1.f };
	PushCoordinateSystem(renderGroup, origin - 0.5f * (xAxis + yAxis), xAxis, yAxis, color,
		&state->testDiffusionTexture, &state->testNormalMap, &tranState->topEnvMap,
		&tranState->middleEnvMap, &tranState->bottomEnvMap);

	for (u32 mapIndex = 0; mapIndex < ArrayCount(maps); mapIndex++) {
		EnvironmentMap* map = maps[mapIndex];
		LoadedBitmap* LOD = map->LOD;

		V2 envMapOrigin = f4(mapIndex) * (V2{ 0, f4(LOD->height) } + V2{ 0, 10.f });
		V2 envMapXAxis = V2i(LOD->width, 0);
		V2 envMapYAxis = V2i(0, LOD->height);
		PushCoordinateSystem(renderGroup, envMapOrigin,
			envMapXAxis, envMapYAxis, V4{ 1, 1, 1, 1 }, LOD, 0, 0, 0, 0);
	}
#endif
	
#if 0
	RenderGroupToBuffer(renderGroup, screenBitmap);
#else
	TiledRenderGroupToBuffer(renderGroup, screenBitmap, tranState->highPriorityQueue);
#endif
	RenderSoundToBuffer(state->audio, tranState->assets, soundData);

	EndSimulation(*simRegion, world);
	EndTempMemory(renderMemory);
	EndTempMemory(simMemory);
	CheckArena(tranState->arena);
	CheckArena(world.arena);
	END_TIMED_SECTION(GameMainLoop);
}