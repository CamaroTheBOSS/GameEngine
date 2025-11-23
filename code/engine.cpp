#include "engine.h"
#include "engine_world.cpp"
#include "engine_simulation.cpp"

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

internal
u32 InitializePlayer(ProgramState* state) {
	EntityStorage storage = {};
	storage.entity.worldPos = GetChunkPositionFromWorldPosition(state->world, 8, 5, 0);
	storage.entity.faceDir = 0;
	storage.entity.type = EntityType_Player;
	storage.entity.size = { state->world.tileSizeInMeters.X * 0.7f,
					state->world.tileSizeInMeters.Y * 0.4f };
	storage.entity.collide = true;
	u32 index = AddEntity(state->world, storage);
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

internal
void MoveEntity(SimRegion& simRegion, ProgramState* state, World& world, Entity& entity, V2 acceleration, f32 dt) {
	V2 moveDelta = 0.5f * acceleration * Squared(dt) + entity.vel * dt;
	entity.vel += acceleration * dt;
	V2 nextPlayerPosition = entity.pos + moveDelta;

	// TODO: G.J.K algorithm for other collision shapes like circles, elipses etc.
	for (u32 iteration = 0; iteration < 4; iteration++) {
		// TODO: now clipping coord value in playerMoveDelta vector happens for the whole playerMoveDelta instead
		// of the rest of the playerMoveDelta after moving to the wall. This causes additional move which should
		// not exists. TODO correct that
		f32 tMin = 1.0f;
		V2 wallNormal = {};
		bool hitCollision = false;
		V2 desiredPosition = entity.pos + moveDelta;
		// TODO: When simRegion is involved, should I have null entity? If yes, change here to 1 and in
		// renderer loop as well
		for (u32 entityIndex = 0; entityIndex < simRegion.entityCount; entityIndex++) {
			Entity* other = simRegion.entities + entityIndex;
			if (!other) {
				continue;
			}

			V2 diff = other->pos - entity.pos;
			V2 minCorner = diff - 0.5f * other->size - 0.5f * entity.size;
			V2 maxCorner = diff + 0.5f * other->size + 0.5f * entity.size;

			if (TestForCollision(maxCorner.X, maxCorner.Y, minCorner.Y, moveDelta.X,
				moveDelta.Y, &tMin)) {
				// Left wall
				wallNormal = { 1.f, 0.f };
				hitCollision = true;
			}
			if (TestForCollision(minCorner.X, maxCorner.Y, minCorner.Y, moveDelta.X,
				moveDelta.Y, &tMin)) {
				// Right wall
				wallNormal = { -1.f, 0.f };
				hitCollision = true;
			}
			if (TestForCollision(maxCorner.Y, maxCorner.X, minCorner.X, moveDelta.Y,
				moveDelta.X, &tMin)) {
				// Bottom wall
				wallNormal = { 0.f, 1.f };
				hitCollision = true;
			}
			if (TestForCollision(minCorner.Y, maxCorner.X, minCorner.X, moveDelta.Y,
				moveDelta.X, &tMin)) {
				// Top wall
				wallNormal = { 0.f, -1.f };
				hitCollision = true;
			}
		}
		entity.pos += moveDelta * tMin;
		if (hitCollision) {
			entity.vel -= Inner(entity.vel, wallNormal) * wallNormal;
			moveDelta = desiredPosition - entity.pos;
			moveDelta -= Inner(moveDelta, wallNormal) * wallNormal;
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
	ChangeEntityChunkLocation(world, world.arena, entity.storageIndex, &entity.worldPos, newEntityPos);
	entity.worldPos = newEntityPos;
}

internal
void SetCamera(ProgramState* state) {
	EntityStorage* cameraEntityStorage = GetEntityStorage(state->world, state->cameraEntityIndex);
	if (cameraEntityStorage) {
		state->cameraPos = OffsetWorldPosition(state->world, state->cameraPos, cameraEntityStorage->entity.pos);
	}
}


extern "C" GAME_MAIN_LOOP_FRAME(GameMainLoopFrame) {
	ProgramState* state = ptrcast(ProgramState, memory.permanentMemory);
	World& world = state->world;
	if (!state->isInitialized) {
		InitializeWorld(world);
		world.arena.data = ptrcast(u8, memory.permanentMemory) + sizeof(ProgramState);
		world.arena.capacity = memory.permanentMemorySize - sizeof(ProgramState);
		world.arena.used = 0;

		state->highFreqBoundHalfDim = 15;
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

		state->isInitialized = true;
	}
	SetCamera(state);
	MemoryArena simArena = {};
	simArena.data = ptrcast(u8, memory.transientMemory);
	simArena.capacity = memory.transientMemorySize;
	Rect2 cameraBounds = GetRectFromCenterHalfDim(
		V2{ 0, 0 }, state->highFreqBoundHalfDim * state->world.tileSizeInMeters.X
	);
	SimRegion* simRegion = BeginSimulation(simArena, world, state->cameraPos, cameraBounds);
	for (u32 playerIdx = 0; playerIdx < MAX_CONTROLLERS; playerIdx++) {
		Controller& controller = controllers[playerIdx];
		u32 playerLowEntityIndex = state->playerEntityIndexes[playerIdx];
		if (controller.isSpaceDown && playerLowEntityIndex == 0) {
			playerLowEntityIndex = InitializePlayer(state);
			state->playerEntityIndexes[playerIdx] = playerLowEntityIndex;
		}

		Entity* entity = GetEntityByStorageIndex(*simRegion, playerLowEntityIndex);
		if (!entity) {
			continue;
		}
		V2 acceleration = {};
		f32 speed = 75.0f;
		if (controller.isADown) {
			entity->faceDir = 1;
			acceleration.X -= 1.f;
		}
		if (controller.isWDown) {
			entity->faceDir = 2;
			acceleration.Y += 1.f;
		}
		if (controller.isSDown) {
			entity->faceDir = 3;
			acceleration.Y -= 1.f;
		}
		if (controller.isDDown) {
			entity->faceDir = 0;
			acceleration.X += 1.f;
		}
		if (controller.isSpaceDown) {
			speed = 250.0f;
		}
		f32 playerAccLength = Length(acceleration);
		if (playerAccLength != 0) {
			acceleration *= speed / Length(acceleration);
		}
		acceleration -= 10.0f * entity->vel;

		MoveEntity(*simRegion, state, world, *entity, acceleration, controller.dtFrame);
	}

	f32 pixelsPerMeter = 42.85714f;
	//pixelsPerMeter = 3.85714f;
	V2 lowerStart = { -world.tileSizeInMeters.X * pixelsPerMeter / 2.0f,
					  scast(f32, bitmap.height) };
	RenderRectangle(bitmap, V2{ 0, 0 }, V2{ scast(f32, bitmap.width), scast(f32, bitmap.height) }, 0.5f, 0.5f, 0.5f);
	for (u32 entityIndex = 0; entityIndex < simRegion->entityCount; entityIndex++) {
		Entity* entity = simRegion->entities + entityIndex;
		if (!entity) {
			continue;
		}

		V2 center = { lowerStart.X + entity->pos.X * pixelsPerMeter + bitmap.width / 2.0f,
					  lowerStart.Y - entity->pos.Y * pixelsPerMeter - bitmap.height / 2.0f };
		V2 min = { center.X - entity->size.X / 2.f * pixelsPerMeter,
				   center.Y - entity->size.Y / 2.f * pixelsPerMeter };
		V2 max = { min.X + entity->size.X * pixelsPerMeter,
				   min.Y + entity->size.Y * pixelsPerMeter };

		if (entity->type == EntityType_Wall) {
#if 0
			i32 index = entity->storageIndex % ArrayCount(randomNumbers);
			u32 randomNum = randomNumbers[index];
			f32 color = scast(f32, randomNum) / scast(f32, 1000000);
			RenderRectangle(bitmap, min, max, color, color, color);
#else
			RenderRectangle(bitmap, min, max, 1.f, 1.f, 1.f);
#endif	
		}
		else if (entity->type == EntityType_Player) {
			RenderRectangle(bitmap, min, max, 0.f, 1.f, 1.f);
			RenderBitmap(bitmap, state->playerMoveAnim[entity->faceDir], V2{min.X, max.Y});
		}
		else {
			RenderRectangle(bitmap, min, max, 0.f, 1.f, 0.f);
		}
	}

	EndSimulation(simArena, *simRegion, world);
}