#include "engine.h"
#include "engine_world.cpp"
#include "engine_simulation.cpp"

constexpr f32 pixelsPerMeter = 42.85714f;

internal
void AddSineWaveToBuffer(SoundData& dst, float amplitude, float toneHz) {
	static u64 runninngSampleIndex = 0;
	float sampleInterval = static_cast<float>(dst.nSamplesPerSec) / (2 * PI * toneHz);
	float* fData = reinterpret_cast<float*>(dst.data);
	for (int frame = 0; frame < dst.nSamples; frame++) {
		dst.tSine += 1.0f / sampleInterval;
#if 0
		float value = amplitude * sinf(dst.tSine);
#else
		float value = 0;
#endif
		for (size_t channel = 0; channel < dst.nChannels; channel++) {
			*fData++ = value;
		}
		runninngSampleIndex++;
	}
	if (dst.tSine > 2 * PI * toneHz) {
		// NOTE: sinf() seems to be inaccurate for high tSine values and quantization goes crazy 
		dst.tSine -= 2 * PI * toneHz;
	}
}

static u32 randomNumbers[] = {
	1706220,	2998326,	7324911,	977285,		7614172,
	5372439,	3343239,	9405090,	493102,		4704943,
	6369811,	4549511,	8835557,	34415,		7580540,
	3802045,	2436711,	3890579,	9233612,	2624780,
	966753,		3838799,	5613984,	9908402,	669180,
	4827667,	5777205,	8909092,	4086663,	558951,
	8469832,	2622484,	4841411,	7100024,	8009380,
	3897537,	9494762,	8461502,	158606,		373031,
	6105258,	1264398,	4643517,	2334477,	7110983,
	7401396,	5884595,	7692621,	5469942,	3114938,
	1622875,	134163,		1809684,	2682374,	8347007,
	5434274,	8352963,	9902538,	9661554,	6635235,
	4486896,	8047031,	3753931,	2112099,	8765638,
	2862273,	2675091,	3117346,	4633880,	7137900,
	6808198,	3644292,	340496,		9395500,	8403385,
	6693618,	1935731,	9761147,	4929305,	1968121,
	2687637,	3132525,	8617055,	9191371,	1655755,
	2420318,	1785622,	1797218,	372211,		1925208,
	6343986,	2421321,	1297186,	2493968,	680611,
	1640785,	896580,		3578674,	680300,		447966
};

inline 
void PushDrawCall(DrawCallGroup& group, LoadedBitmap* bitmap, Rect2 rectangle, f32 R, f32 G, f32 B, f32 A, V2 offset) {
	Assert(group.count < ArrayCount(group.drawCalls));
	DrawCall* call = &group.drawCalls[group.count++];
	call->bitmap = bitmap;
	call->rectangle = rectangle;
	call->R = R;
	call->G = G;
	call->B = B;
	call->A = A;
	call->offset = offset;
}

inline
void PushBitmap(DrawCallGroup& group, LoadedBitmap* bitmap, f32 A, V2 offset) {
	PushDrawCall(group, bitmap, {}, 0, 0, 0, A, offset);
}

inline
void PushRect(DrawCallGroup& group, Rect2 rect, f32 R, f32 G, f32 B, f32 A, V2 offset) {
	PushDrawCall(group, 0, rect, R, G, B, A, offset);
}

internal
void RenderHitPoints(DrawCallGroup& group, Entity& entity, V2 center, V2 offset, f32 distBetween, f32 pointSize) {
	V2 realOffset = (offset + V2{ -scast(f32, entity.hitPoints.count - 1) * scast(f32, distBetween + pointSize) / 2.f , 0.f }) * pixelsPerMeter;
	for (u32 hitPointIndex = 0; hitPointIndex < entity.hitPoints.count; hitPointIndex++) {
		V2 hitPointMin = { center.X + realOffset.X - pointSize * 0.5f * pixelsPerMeter,
						   center.Y - realOffset.Y - pointSize * 0.5f * pixelsPerMeter, };
		V2 hitPointMax = { center.X + realOffset.X + pointSize * 0.5f * pixelsPerMeter,
						   center.Y - realOffset.Y + pointSize * 0.5f * pixelsPerMeter, };
		PushRect(group, GetRectFromMinMax(hitPointMin, hitPointMax), 1.f, 0.f, 0.f, 1.f, {});
		realOffset.X += (distBetween + pointSize) * pixelsPerMeter;
	}
}

internal 
void RenderRectangle(BitmapData& bitmap, V2 start, V2 end, f32 R, f32 G, f32 B) {
	i32 minX = RoundF32ToI32(start.X);
	i32 maxX = RoundF32ToI32(end.X);
	i32 minY = RoundF32ToI32(start.Y);
	i32 maxY = RoundF32ToI32(end.Y);
	if (minX < 0) {
		minX = 0;
	}
	if (minY < 0) {
		minY = 0;
	}
	if (maxX > scast(i32, bitmap.width)) {
		maxX = scast(i32, bitmap.width);
	}
	if (maxY > scast(i32, bitmap.height)) {
		maxY = scast(i32, bitmap.height);
	}
	u32 color = (scast(u32, 255 * R) << 16) +
				(scast(u32, 255 * G) << 8) +
				(scast(u32, 255 * B) << 0);
	u8* row = ptrcast(u8, bitmap.data) + minY * bitmap.pitch + minX * bitmap.bytesPerPixel;
	for (i32 Y = minY; Y < maxY; Y++) {
		u32* pixel = ptrcast(u32, row);
		for (i32 X = minX; X < maxX; X++) {
			*pixel++ = color;
		}
		row += bitmap.pitch;
	}
}

internal
void RenderBitmap(BitmapData& screenBitmap, LoadedBitmap& loadedBitmap, V2 position) {
	i32 minX = RoundF32ToI32(position.X) - loadedBitmap.alignX;
	i32 maxX = minX + loadedBitmap.width;
	i32 minY = RoundF32ToI32(position.Y) - loadedBitmap.alignY;
	i32 maxY = minY + loadedBitmap.height;
	i32 offsetX = 0;
	i32 offsetY = 0;
	if (minX < 0) {
		offsetX = -minX;
		minX = 0;
	}
	if (minY < 0) {
		offsetY = -minY;
		minY = 0;
	}
	if (maxX > scast(i32, screenBitmap.width)) {
		maxX = scast(i32, screenBitmap.width);
	}
	if (maxY > scast(i32, screenBitmap.height)) {
		maxY = scast(i32, screenBitmap.height);
	}
	u32 loadedBitmapPitch = loadedBitmap.width * loadedBitmap.bytesPerPixel;
	u8* dstRow = ptrcast(u8, screenBitmap.data) + minY * screenBitmap.pitch + minX * screenBitmap.bytesPerPixel;
	u8* srcRow = ptrcast(u8, loadedBitmap.data + (loadedBitmap.height - 1 - offsetY) * (loadedBitmap.width) + offsetX);
	for (i32 Y = minY; Y < maxY; Y++) {
		u32* dstPixel = ptrcast(u32, dstRow);
		u32* srcPixel = ptrcast(u32, srcRow);
		for (i32 X = minX; X < maxX; X++) {
			f32 dR = scast(f32, (*dstPixel >> 16) & 0xFF);
			f32 dG = scast(f32, (*dstPixel >> 8) & 0xFF);
			f32 dB = scast(f32, (*dstPixel >> 0) & 0xFF);

			f32 sA = scast(f32, (*srcPixel >> 24) & 0xFF) / 255.f;
			f32 sR = scast(f32, (*srcPixel >> 16) & 0xFF);
			f32 sG = scast(f32, (*srcPixel >> 8) & 0xFF);
			f32 sB = scast(f32, (*srcPixel >> 0) & 0xFF);

			u8 r = scast(u8, sA * sR + (1 - sA) * dR);
			u8 g = scast(u8, sA * sG + (1 - sA) * dG);
			u8 b = scast(u8, sA * sB + (1 - sA) * dB);

			*dstPixel++ = (r << 16) | (g << 8) | (b << 0);
			srcPixel++;
		}
		dstRow += screenBitmap.pitch;
		srcRow -= loadedBitmapPitch;
	}
}


internal
void RenderWeirdGradient(BitmapData& bitmap, int xOffset, int yOffset) {
	u8* row = reinterpret_cast<u8*>(bitmap.data);
	for (u32 y = 0; y < bitmap.height; y++) {
		u8* pixel = reinterpret_cast<u8*>(row);
		for (u32 x = 0; x < bitmap.width; x++) {
			pixel[0] = static_cast<u8>(x + xOffset);
			pixel[1] = static_cast<u8>(y + yOffset);
			pixel[2] = 0;
			pixel[3] = 0;
			pixel += 4;
		}
		row += bitmap.pitch;
	}
}

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

internal
LoadedBitmap LoadBmpFile(debug_read_entire_file* debugReadEntireFile, const char* filename) {
	FileData bmpData = debugReadEntireFile(filename);
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
	result.data = ptrcast(u32, ptrcast(u8, bmpData.content) + header->bitmapOffset);
	result.height = header->height;
	result.width = header->width;
	result.bytesPerPixel = header->bitsPerPixel / 8;

	u32* pixels = result.data;
	for (u32 Y = 0; Y < header->height; Y++) {
		for (u32 X = 0; X < header->width; X++) {
			u32 A = (*pixels >> alphaShift) & 0xFF;
			u32 R = (*pixels >> redShift) & 0xFF;
			u32 G = (*pixels >> greenShift) & 0xFF;
			u32 B = (*pixels >> blueShift) & 0xFF;
			*pixels++ = (A << 24) + (R << 16) + (G << 8) + (B << 0);
		}
	}
	
	return result;
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
u32 AddWall(World& world, u32 absX, u32 absY, u32 absZ) {
	// TODO: Instead of changing world position into chunk position for all the objects just
	// call AddEntity() which will do it for ourselves
	EntityStorage storage = {};
	storage.entity.worldPos = GetChunkPositionFromWorldPosition(world, absX, absY, absZ);
	storage.entity.size = V2{ world.tileSizeInMeters.X, world.tileSizeInMeters.Y };
	SetFlag(storage.entity, EntityFlag_StopsOnCollide);
	storage.entity.type = EntityType_Wall;
	return AddEntity(world, storage);
}

internal
u32 AddFamiliar(World& world, u32 absX, u32 absY, u32 absZ) {
	EntityStorage storage = {};
	storage.entity.worldPos = GetChunkPositionFromWorldPosition(world, absX, absY, absZ);
	storage.entity.size = V2{ 1.0f, 1.25f };
	storage.entity.type = EntityType_Familiar;
	return AddEntity(world, storage);
}

internal
u32 AddSword(World& world) {
	EntityStorage storage = {};
	storage.entity.worldPos = NullPosition();
	storage.entity.size = V2{ 0.5f, 0.5f };
	storage.entity.type = EntityType_Sword;
	return AddEntity(world, storage);
}

internal
u32 AddMonster(World& world, u32 absX, u32 absY, u32 absZ) {
	EntityStorage storage = {};
	storage.entity.worldPos = GetChunkPositionFromWorldPosition(world, absX, absY, absZ);
	storage.entity.size = V2{ 1.0f, 1.25f };
	SetFlag(storage.entity, EntityFlag_StopsOnCollide);
	storage.entity.type = EntityType_Monster;
	InitializeHitPoints(storage.entity, 3, 1, 1);
	return AddEntity(world, storage);
}

internal
u32 InitializePlayer(ProgramState* state) {
	EntityStorage storage = {};
	storage.entity.worldPos = GetChunkPositionFromWorldPosition(state->world, 8, 5, 0);
	storage.entity.faceDir = 0;
	storage.entity.type = EntityType_Player;
	storage.entity.size = { state->world.tileSizeInMeters.X * 0.7f,
					state->world.tileSizeInMeters.Y * 0.4f };
	SetFlag(storage.entity, EntityFlag_StopsOnCollide);
	InitializeHitPoints(storage.entity, 4, 1, 1);
	u32 swIndex = AddSword(state->world);
	EntityStorage* swordStorage = GetEntityStorage(state->world, swIndex);
	swordStorage->entity.storageIndex = swIndex;
	storage.entity.sword = &swordStorage->entity;
	Assert(storage.entity.sword);
	u32 index = AddEntity(state->world, storage);
	Assert(index);
	if (!state->cameraEntityIndex) {
		state->cameraEntityIndex = index;
	}
	return index;
}

bool TestForCollision(f32 maxCornerX, f32 maxCornerY, f32 minCornerY, f32 moveDeltaX,
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
		newPair = ptrcast(PairwiseCollision, PushStructSize(arena, PairwiseCollision));
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

internal
bool HandleCollision(World& world, Entity& first, Entity& second) {
	bool stopOnCollide = (IsFlagSet(first, EntityFlag_StopsOnCollide) && IsFlagSet(second, EntityFlag_StopsOnCollide));
	if (!ShouldCollide(world, first.storageIndex, second.storageIndex)) {
		return stopOnCollide;
	}
	if (first.type == EntityType_Sword && second.type == EntityType_Monster) {
		if (second.hitPoints.count > 0) {
			second.hitPoints.count--;
		}
	}
	AddCollisionRule(world, world.arena, first.storageIndex, second.storageIndex);
	AddCollisionRule(world, world.arena, second.storageIndex, first.storageIndex);
	return stopOnCollide;
}

internal
void MoveEntity(SimRegion& simRegion, ProgramState* state, World& world, Entity& entity, V3 acceleration, f32 dt) {
	if (IsFlagSet(entity, EntityFlag_NonSpatial)) {
		return;
	}
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
		V3 wallNormal = {};
		bool hitCollision = false;
		V3 desiredPosition = entity.pos + moveDelta;
		Entity* hitEntity = 0;

		for (u32 entityIndex = 0; entityIndex < simRegion.entityCount; entityIndex++) {
			Entity* other = simRegion.entities + entityIndex;
			if (!other || IsFlagSet(*other, EntityFlag_NonSpatial)) {
				continue;
			}

			V3 diff = other->pos - entity.pos;
			V2 minCorner = diff.XY - 0.5f * other->size - 0.5f * entity.size;
			V2 maxCorner = diff.XY + 0.5f * other->size + 0.5f * entity.size;

			if (TestForCollision(maxCorner.X, maxCorner.Y, minCorner.Y, moveDelta.X,
				moveDelta.Y, &tMin)) {
				// Left wall
				wallNormal = { 1.f, 0.f, 0.f };
				hitCollision = true;
				hitEntity = other;
			}
			if (TestForCollision(minCorner.X, maxCorner.Y, minCorner.Y, moveDelta.X,
				moveDelta.Y, &tMin)) {
				// Right wall
				wallNormal = { -1.f, 0.f, 0.f };
				hitCollision = true;
				hitEntity = other;
			}
			if (TestForCollision(maxCorner.Y, maxCorner.X, minCorner.X, moveDelta.Y,
				moveDelta.X, &tMin)) {
				// Bottom wall
				wallNormal = { 0.f, 1.f, 0.f };
				hitCollision = true;
				hitEntity = other;
			}
			if (TestForCollision(minCorner.Y, maxCorner.X, minCorner.X, moveDelta.Y,
				moveDelta.X, &tMin)) {
				// Top wall
				wallNormal = { 0.f, -1.f, 0.f };
				hitCollision = true;
				hitEntity = other;
			}
		}

		entity.pos += moveDelta * tMin;
		if (entity.distanceRemaining != 0.f) {
			constexpr f32 dEpsilon = 0.01f;
			entity.distanceRemaining -= Length(moveDelta * tMin);
			if (entity.distanceRemaining < dEpsilon) {
				entity.distanceRemaining = 0.f;
			}
		}
		if (hitCollision) {
			Assert(hitEntity);
			bool stopOnCollision = HandleCollision(world, entity, *hitEntity);
			if (stopOnCollision) {
				entity.vel -= Inner(entity.vel, wallNormal) * wallNormal;
				moveDelta = desiredPosition - entity.pos;
				moveDelta -= Inner(moveDelta, wallNormal) * wallNormal;
			}
			else {
				// TODO: Can it be done in a better, smarter way?
				entity.pos += moveDelta * (1 - tMin);
			}
		}
		if (tMin == 1.0f) {
			break;
		}
	}
	WorldPosition newEntityPos = OffsetWorldPosition(world, state->cameraPos, entity.pos);
	// TODO: change location of ChangeEntityChunkLocation, it can be potentially problem in the future
	// cause this changes location of low entity which is not updated by EndSimulation(), so it looks
	// like it has been moved by until EndSimulation() is not called its data is not in sync with chunk
	// location
	ChangeEntityChunkLocation(world, world.arena, entity.storageIndex, entity, &entity.worldPos, newEntityPos);
}

internal
void UpdateFamiliar(SimRegion& simRegion, ProgramState* state, Entity* familiar, f32 dt) {
	f32 minDistance = Squared(10.f);
	V3 minDistanceEntityPos = {};
	for (u32 entityIndex = 0; entityIndex < simRegion.entityCount; entityIndex++) {
		Entity* entity = simRegion.entities + entityIndex;
		if (entity->type == EntityType_Player) {
			f32 distance = LengthSq(entity->pos - familiar->pos);
			if (distance < minDistance) {
				minDistance = distance;
				minDistanceEntityPos = entity->pos;
			}
		}
	}
	V3 acceleration = {};
	f32 speed = 50.0f;
	if (minDistance > Squared(2.0f)) {
		acceleration = speed * (minDistanceEntityPos - familiar->pos) / SquareRoot(minDistance);
		acceleration.Z = 0.f;
	}
	acceleration -= 10.0f * familiar->vel;
	MoveEntity(simRegion, state, state->world, *familiar, acceleration, dt);
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
	if (IsFlagSet(entity, EntityFlag_NonSpatial)) {
		ChangeEntityChunkLocation(world, world.arena, storageEntityIndex, entity, 0, newPos);
		TryAddEntityToSim(simRegion, world, storageEntityIndex, entity);
	}
}

internal
void MakeEntityNonSpatial(ProgramState* state, u32 storageEntityIndex, Entity& entity) {
	if (!IsFlagSet(entity, EntityFlag_NonSpatial)) {
		WorldPosition nullPosition = NullPosition();
		ChangeEntityChunkLocation(state->world, state->world.arena, storageEntityIndex, entity, &entity.worldPos, nullPosition);
	}
}

internal
V2 MapScreenSpacePosIntoCameraSpace(f32 screenPosX, f32 screenPosY, u32 screenWidth, u32 screenHeight) {
	V2 pos = {};
	pos.X = screenWidth * (screenPosX - 0.5f) / pixelsPerMeter;
	pos.Y = screenHeight * (0.5f - screenPosY) / pixelsPerMeter;
	return pos;
}


extern "C" GAME_MAIN_LOOP_FRAME(GameMainLoopFrame) {
	ProgramState* state = ptrcast(ProgramState, memory.permanentMemory);
	World& world = state->world;
	if (!state->isInitialized) {
		InitializeWorld(world);
		world.arena.data = ptrcast(u8, memory.permanentMemory) + sizeof(ProgramState);
		world.arena.capacity = memory.permanentMemorySize - sizeof(ProgramState);
		world.arena.used = 0;

		state->highFreqBoundDim = 30;
		state->highFreqBoundHeight = 1;
		state->cameraPos = GetChunkPositionFromWorldPosition(
			world, world.tileCountX / 2, world.tileCountY / 2, 0
		);

		state->playerMoveAnim[0] = LoadBmpFile(memory.debugReadEntireFile, "test/hero-right.bmp");
		state->playerMoveAnim[0].alignX = 0;
		state->playerMoveAnim[0].alignY = 55;

		state->playerMoveAnim[1] = LoadBmpFile(memory.debugReadEntireFile, "test/hero-left.bmp");
		state->playerMoveAnim[1].alignX = 0;
		state->playerMoveAnim[1].alignY = 55;

		state->playerMoveAnim[2] = LoadBmpFile(memory.debugReadEntireFile, "test/hero-up.bmp");
		state->playerMoveAnim[2].alignX = 0;
		state->playerMoveAnim[2].alignY = 55;

		state->playerMoveAnim[3] = LoadBmpFile(memory.debugReadEntireFile, "test/hero-down.bmp");
		state->playerMoveAnim[3].alignX = 0;
		state->playerMoveAnim[3].alignY = 55;

		//TODO pixelsPerMeters as world property?

		bool doorLeft = false;
		bool doorRight = false;
		bool doorUp = false;
		bool doorDown = false;
		bool lvlJustChanged = false;
		bool ladder = false;
		u32 screenX = 0;
		u32 screenY = 0;
		u32 randomNIdx = 0;
		u32 absTileZ = 0;
		for (u32 screenIndex = 0; screenIndex < 100; screenIndex++) {
			randomNIdx = (randomNIdx + 1) % ArrayCount(randomNumbers);
			u32 randomNumber = randomNumbers[randomNIdx];
#if 0
			u32 mod = randomNumber % 3;
#else
			u32 mod = randomNumber % 2;
#endif
			if (ladder) {
				mod = randomNumber % 2;
				ladder = false;
			}
			if (mod == 0) {
				doorRight = true;
			}
			else if (mod == 1) {
				doorUp = true;
			}
			else if (mod == 2) {
				ladder = true;
			}

			for (u32 tileY = 0; tileY < world.tileCountY; tileY++) {
				for (u32 tileX = 0; tileX < world.tileCountX; tileX++) {
					u32 absTileX = screenX * world.tileCountX + tileX;
					u32 absTileY = screenY * world.tileCountY + tileY;

					u32 tileValue = 1;
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
					if (ladder && absTileZ == 0 && tileX == 1 && tileY == 1) {
						tileValue = 3; // Ladder up
					}
					else if (ladder && absTileZ == 1 && tileX == 2 && tileY == 2) {
						tileValue = 4; // Ladder down
					}
					else if (lvlJustChanged && absTileZ == 0 && tileX == 1 && tileY == 1) {
						tileValue = 3; // Ladder up
					}
					else if (lvlJustChanged && absTileZ == 1 && tileX == 2 && tileY == 2) {
						tileValue = 4; // Ladder up
					}
					// TODO: Chunk allocation on demand
					if (tileValue == 2) {
						AddWall(world, absTileX, absTileY, absTileZ);
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
			if (ladder) {
				lvlJustChanged = true;
				absTileZ = scast(u32, !absTileZ);
			}
			else {
				lvlJustChanged = false;
			}
		}
		AddFamiliar(world, 17 / 2, 9 / 2, 0);
		AddMonster(world, 17 / 2, 7, 0);
		//AddWall(world, 3, 3, 14);

		state->isInitialized = true;
	}
	SetCamera(state);
	MemoryArena simArena = {};
	simArena.data = ptrcast(u8, memory.transientMemory);
	simArena.capacity = memory.transientMemorySize;
	V3 cameraBoundsDims = {
		state->highFreqBoundDim * state->world.tileSizeInMeters.X,
		state->highFreqBoundDim * state->world.tileSizeInMeters.Y,
		state->highFreqBoundHeight * state->world.tileSizeInMeters.Z
	};
	Rect3 cameraBounds = GetRectFromCenterDim(V3{ 0, 0, 0 }, cameraBoundsDims);
	SimRegion* simRegion = BeginSimulation(simArena, world, state->cameraPos, cameraBounds);
	for (u32 playerIdx = 0; playerIdx < MAX_CONTROLLERS; playerIdx++) {
		Controller& controller = input.controllers[playerIdx];
		PlayerControls& playerControls = state->playerControls[playerIdx];
		u32 playerLowEntityIndex = state->playerEntityIndexes[playerIdx];
		if (controller.isSpaceDown && playerLowEntityIndex == 0) {
			playerLowEntityIndex = InitializePlayer(state);
			state->playerEntityIndexes[playerIdx] = playerLowEntityIndex;
		}

		Entity* entity = GetEntityByStorageIndex(*simRegion, playerLowEntityIndex);
		if (!entity) {
			continue;
		}
		playerControls.acceleration = {};
		f32 speed = 75.0f;
		if (controller.isADown) {
			entity->faceDir = 1;
			playerControls.acceleration.X -= 1.f;
		}
		if (controller.isWDown) {
			entity->faceDir = 2;
			playerControls.acceleration.Y += 1.f;
		}
		if (controller.isSDown) {
			entity->faceDir = 3;
			playerControls.acceleration.Y -= 1.f;
		}
		if (controller.isDDown) {
			entity->faceDir = 0;
			playerControls.acceleration.X += 1.f;
		}
		if (controller.isSpaceDown) {
			speed = 250.0f;
		}
		if (controller.isMouseLDown) {
			entity->sword->distanceRemaining = 5.f;
			V2 mousePos = MapScreenSpacePosIntoCameraSpace(controller.mouseX, controller.mouseY, bitmap.width, bitmap.height);
			//mousePos -= entity->sword->size / 2.f;
			
#if 0
			EntityStorage storage = {};
			storage.entity.worldPos = OffsetWorldPosition(world, state->cameraPos, mousePos);
			storage.entity.size = world.tileSizeInMeters;
			SetFlag(storage.entity, EntityFlag_Collide);
			storage.entity.type = EntityType_Wall;
			AddEntity(world, storage);
#endif
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
		f32 playerAccLength = Length(playerControls.acceleration);
		if (playerAccLength != 0) {
			playerControls.acceleration *= speed / Length(playerControls.acceleration);
		}
		playerControls.acceleration -= 10.0f * entity->vel;
	}

	RenderRectangle(bitmap, V2{ 0, 0 }, V2{ scast(f32, bitmap.width), scast(f32, bitmap.height) }, 0.5f, 0.5f, 0.5f);
	for (u32 entityIndex = 0; entityIndex < simRegion->entityCount; entityIndex++) {
		Entity* entity = simRegion->entities + entityIndex;
		if (!entity) {
			continue;
		}
		DrawCallGroup drawCalls = {};

		V2 center = { entity->pos.X * pixelsPerMeter + bitmap.width / 2.0f,
					  scast(f32, bitmap.height) - entity->pos.Y * pixelsPerMeter - bitmap.height / 2.0f };
		V2 min = { center.X - entity->size.X / 2.f * pixelsPerMeter,
				   center.Y - entity->size.Y / 2.f * pixelsPerMeter };
		V2 max = { min.X + entity->size.X * pixelsPerMeter,
				   min.Y + entity->size.Y * pixelsPerMeter };

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
			V3 acceleration = playerControls->acceleration;
			MoveEntity(*simRegion, state, world, *entity, acceleration, input.dtFrame);
			PushRect(drawCalls, GetRectFromMinMax(min, max), 0.f, 1.f, 1.f, 1.f, {});
			PushBitmap(drawCalls, &state->playerMoveAnim[entity->faceDir], 1.f, V2{ min.X, max.Y });
			RenderHitPoints(drawCalls, *entity, center, V2{0.f, -0.5f}, 0.1f, 0.2f);
		} break;
		case EntityType_Wall: {
			PushRect(drawCalls, GetRectFromMinMax(min, max), 1.f, 1.f, 1.f, 1.f, {});
			RenderRectangle(bitmap, min, max, 1.f, 1.f, 1.f);
		} break;
		case EntityType_Familiar: {
			UpdateFamiliar(*simRegion, state, entity, input.dtFrame);
			PushRect(drawCalls, GetRectFromMinMax(min, max), 0.f, 0.f, 1.f, 1.f, {});
		} break;
		case EntityType_Monster: {
			PushRect(drawCalls, GetRectFromMinMax(min, max), 1.f, 0.5f, 0.f, 1.f, {});
			RenderHitPoints(drawCalls, *entity, center, V2{ 0.f, -0.8f }, 0.1f, 0.2f);
		} break;
		case EntityType_Sword: {
			PushRect(drawCalls, GetRectFromMinMax(min, max), 0.f, 0.f, 0.f, 1.f, {});
			if (entity->distanceRemaining <= 0.f) {
				ClearCollisionRuleForEntity(state->world, entity->storageIndex);
				MakeEntityNonSpatial(state, entity->storageIndex, *entity);
			}
			MoveEntity(*simRegion, state, world, *entity, V3{ 0.f, 0.f, 0.f }, input.dtFrame);
		} break;
		default: Assert(!"Function to draw entity not found!");
		}

		for (u32 drawCallIndex = 0; drawCallIndex < drawCalls.count; drawCallIndex++) {
			DrawCall* call = drawCalls.drawCalls + drawCallIndex;
			if (call->bitmap) {
				RenderBitmap(bitmap, *call->bitmap, call->offset);
			}
			else {
				RenderRectangle(bitmap, GetMinCorner(call->rectangle), GetMaxCorner(call->rectangle), call->R, call->G, call->B);
			}
		}
	}

	EndSimulation(simArena, *simRegion, world);
}