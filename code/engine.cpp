#include "engine.h"
#include "engine_world.cpp"

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

inline internal
u32 AddEntity(ProgramState* state, LowEntity& low) {
	if (state->lowEntityCount < ArrayCount(state->lowEntities)) {
		state->lowEntities[state->lowEntityCount++] = low;
		return state->lowEntityCount - 1;
	}
	return 0;
}

inline
u32 AddWall(ProgramState* state, u32 absX, u32 absY, u32 absZ) {
	LowEntity low = {};
	low.pos.absX = absX;
	low.pos.absY = absY;
	low.pos.absZ = absZ;
	low.size = state->world.tilemap.tileSizeInMeters;
	low.type = EntityType_Wall;
	return AddEntity(state, low);
}

internal
LowEntity* GetEntity(ProgramState* state, u32 lowEntityIndex) {
	LowEntity* entity = 0;
	if (lowEntityIndex > 0 && lowEntityIndex < state->lowEntityCount) {
		entity = &state->lowEntities[lowEntityIndex];
	}
	return entity;
}

internal
HighEntity* MakeEntityHighFrequency(ProgramState* state, u32 lowEntityIndex) {
	HighEntity* result = 0;
	if (state->highEntityCount >= ArrayCount(state->highEntities)) {
		return result;
	}
	LowEntity* low = GetEntity(state, lowEntityIndex);
	if (low->highEntityIndex > 0) {
		return &state->highEntities[state->highEntityCount];
	}
	DiffTilePosition diff = Subtract(state->world.tilemap, low->pos, state->cameraPos);
	HighEntity high = {};
	high.pos = diff.dXY;
	high.vel = V2{ 0, 0 };
	high.lowEntityIndex = lowEntityIndex;
	
	state->highEntities[state->highEntityCount] = high;
	low->highEntityIndex = state->highEntityCount;
	state->highEntityCount++;
	return &state->highEntities[state->highEntityCount];
}

internal
u32 InitializePlayer(ProgramState* state) {
	LowEntity low = {};
	low.pos.absX = 3;
	low.pos.absY = 3;
	low.pos.absZ = 0;
	low.pos.offset = V2{ 0, 0 };
	low.faceDir = 0;
	low.type = EntityType_Player;
	low.size = { state->world.tilemap.tileSizeInMeters.X * 0.7f,
					state->world.tilemap.tileSizeInMeters.Y * 0.9f };
	u32 index = AddEntity(state, low);
	MakeEntityHighFrequency(state, index);
	if (!state->cameraEntityIndex) {
		state->cameraEntityIndex = index;
	}
	return index;
}

bool TestForCollision(f32 maxCornerX, f32 maxCornerY, f32 minCornerY, f32 moveDeltaX,
					  f32 moveDeltaY, f32* tMin) {
	if (moveDeltaX != 0) {
		f32 tEpsilon = 0.0001f;
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
void MovePlayer(ProgramState* state, TileMap& tilemap, Controller& controller, HighEntity* playerHigh) {
	// TODO can playerHigh be just reference?
	if (!playerHigh) {
		return;
	}
	LowEntity* playerLow = GetEntity(state, playerHigh->lowEntityIndex);
	Assert(playerLow && &state->highEntities[playerLow->highEntityIndex] == playerHigh);
	if (!playerLow) {
		return;
	}
	V2 playerAcceleration = {};
	f32 speed = 75.0f;
	if (controller.isADown) {
		playerLow->faceDir = 1;
		playerAcceleration.X -= 1.f;
	}
	if (controller.isWDown) {
		playerLow->faceDir = 2;
		playerAcceleration.Y += 1.f;
	}
	if (controller.isSDown) {
		playerLow->faceDir = 3;
		playerAcceleration.Y -= 1.f;
	}
	if (controller.isDDown) {
		playerLow->faceDir = 0;
		playerAcceleration.X += 1.f;
	}
	if (controller.isSpaceDown) {
		speed = 250.0f;
	}
	f32 playerAccLength = Length(playerAcceleration);
	if (playerAccLength != 0) {
		playerAcceleration *= speed / Length(playerAcceleration);
	}
	playerAcceleration -= 10.0f * playerHigh->vel;

	V2 playerMoveDelta = 0.5f * playerAcceleration * Squared(controller.dtFrame) +
						 playerHigh->vel * controller.dtFrame;
	
#if 1
	playerHigh->pos += playerMoveDelta;
	playerHigh->vel += playerAcceleration * controller.dtFrame;
#else
	playerHigh->vel += playerAcceleration * controller.dtFrame;
	TilePosition nextPlayerPosition = OffsetPosition(tilemap, player->low->pos, playerMoveDelta);
	V2 collisionRadius = { playerLow->size.X / 2.f,
						   playerLow->size.Y / 5.f };

	// TODO: G.J.K algorithm for other collision shapes like circles, elipses etc.

	i32 minX = scast(i32, Minimum(player->low->pos.absX, nextPlayerPosition.absX)) - CeilF32ToU32(collisionRadius.X);
	i32 minY = scast(i32, Minimum(player->low->pos.absY, nextPlayerPosition.absY)) - CeilF32ToU32(collisionRadius.X);
	i32 maxX = scast(i32, Maximum(player->low->pos.absX, nextPlayerPosition.absX)) + CeilF32ToU32(collisionRadius.Y) + 1;
	i32 maxY = scast(i32, Maximum(player->low->pos.absY, nextPlayerPosition.absY)) + CeilF32ToU32(collisionRadius.Y) + 1;
	Assert(maxX - minX < 32);
	Assert(maxY - minY < 32);
	f32 tRemaining = 1.0f;
	V2 totalPlayerMoveDelta = {};
	for (u32 iteration = 0; iteration < 4 && tRemaining > 0.0f; iteration++) {
		// TODO: now clipping coord value in playerMoveDelta vector happens for the whole playerMoveDelta instead
		// of the rest of the playerMoveDelta after moving to the wall. This causes additional move which should
		// not exists. TODO correct that
		f32 tMin = 1.0f;
		V2 wallNormal = {};
		for (i32 tileY = minY; tileY < maxY; tileY++) {
			for (i32 tileX = minX; tileX < maxX; tileX++) {
				TilePosition tilePos = CenteredTilePosition(scast(u32, tileX), scast(u32, tileY), player->low->pos.absZ);
				u32 tileValue = GetTileValue(tilemap, tilePos);
				if (IsTileValueEmpty(tileValue)) {
					continue;
				}
				DiffTilePosition diff = Subtract(tilemap, tilePos, playerPosForCollision);

				V2 minCorner = diff.dXY - 0.5f * tilemap.tileSizeInMeters - collisionRadius;
				V2 maxCorner = diff.dXY + 0.5f * tilemap.tileSizeInMeters + collisionRadius;

				if (TestForCollision(maxCorner.X, maxCorner.Y, minCorner.Y, playerMoveDelta.X,
					playerMoveDelta.Y, &tMin)) {
					// Left wall
					wallNormal = { 1.f, 0.f };
				}
				if (TestForCollision(minCorner.X, maxCorner.Y, minCorner.Y, playerMoveDelta.X,
					playerMoveDelta.Y, &tMin)) {
					// Right wall
					wallNormal = { -1.f, 0.f };
				}
				if (TestForCollision(maxCorner.Y, maxCorner.X, minCorner.X, playerMoveDelta.Y,
					playerMoveDelta.X, &tMin)) {
					// Bottom wall
					wallNormal = { 0.f, 1.f };
				}
				if (TestForCollision(minCorner.Y, maxCorner.X, minCorner.X, playerMoveDelta.Y,
					playerMoveDelta.X, &tMin)) {
					// Top wall
					wallNormal = { 0.f, -1.f };
				}
			}
		}
		totalPlayerMoveDelta += playerMoveDelta * tMin;
		playerPosForCollision = OffsetPosition(tilemap, playerPosForCollision, playerMoveDelta * tMin);
		player->high->vel -= Inner(player->high->vel, wallNormal) * wallNormal;
		playerMoveDelta -= Inner(playerMoveDelta, wallNormal) * wallNormal;
		tRemaining -= tMin;
	}


	bool theSameTile = AreOnTheSameTile(player->low->pos, nextPlayerPosition);
	if (!theSameTile) {
		u32 tileValue = GetTileValue(tilemap, nextPlayerPosition);
		if (tileValue == 3) {
			player->low->pos.absZ = 1;
		}
		else if (tileValue == 4) {
			player->low->pos.absZ = 0;
		}
	}

	player->low->pos = OffsetPosition(tilemap, player->low->pos, totalPlayerMoveDelta);
#if HANDMADE_SLOW
	TilePosition rightTopCorner = OffsetPosition(tilemap, playerPosForCollision, collisionRadius.X, collisionRadius.Y);
	TilePosition leftTopCorner = OffsetPosition(tilemap, playerPosForCollision, -collisionRadius.X, collisionRadius.Y);
	TilePosition rightBotCorner = OffsetPosition(tilemap, playerPosForCollision, collisionRadius.X, -collisionRadius.Y);
	TilePosition leftBotCorner = OffsetPosition(tilemap, playerPosForCollision, -collisionRadius.X, -collisionRadius.Y);
	Assert(IsTileValueEmpty(GetTileValue(tilemap, rightTopCorner)));
	Assert(IsTileValueEmpty(GetTileValue(tilemap, leftTopCorner)));
	Assert(IsTileValueEmpty(GetTileValue(tilemap, rightBotCorner)));
	Assert(IsTileValueEmpty(GetTileValue(tilemap, leftBotCorner)));
#endif
#endif
}


extern "C" GAME_MAIN_LOOP_FRAME(GameMainLoopFrame) {
	ProgramState* state = ptrcast(ProgramState, memory.permanentMemory);
	TileMap& tilemap = state->world.tilemap;
	if (!state->isInitialized) {
		state->highEntityCount = 1;
		state->lowEntityCount = 1;
		
		state->worldArena.data = ptrcast(u8, memory.permanentMemory) + sizeof(ProgramState);
		state->worldArena.capacity = memory.permanentMemorySize - sizeof(ProgramState);
		state->worldArena.used = 0;

		tilemap.tileCountX = 17;
		tilemap.tileCountY = 9;
		tilemap.tileSizeInMeters = V2{ 1.4f , 1.4f };
		tilemap.chunkCountX = 32;
		tilemap.chunkCountY = 32;
		tilemap.chunkCountZ = 2;
		tilemap.chunkShift = 4;
		tilemap.chunkMask = (1 << tilemap.chunkShift) - 1;
		tilemap.chunkDim = (1 << tilemap.chunkShift);
		tilemap.tileChunks = ptrcast(TileChunk, PushArray(
			state->worldArena,
			tilemap.chunkCountX * tilemap.chunkCountY * tilemap.chunkCountZ,
			TileChunk
		));

		state->cameraPos.absX = tilemap.tileCountX / 2;
		state->cameraPos.absY = tilemap.tileCountY / 2;
		state->cameraPos.absZ = 0;
		state->cameraPos.offset = V2{0, 0};

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
		for (u32 screenIndex = 0; screenIndex < 2; screenIndex++) {
			randomNIdx = (randomNIdx + 1) % ArrayCount(randomNumbers);
			u32 randomNumber = randomNumbers[randomNIdx];
			u32 mod = randomNumber % 3;
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

			for (u32 tileY = 0; tileY < tilemap.tileCountY; tileY++) {
				for (u32 tileX = 0; tileX < tilemap.tileCountX; tileX++) {
					u32 absTileX = screenX * tilemap.tileCountX + tileX;
					u32 absTileY = screenY * tilemap.tileCountY + tileY;

					u32 tileValue = 1;
					if (tileX == 0) {
						tileValue = 2;
						if (doorLeft && tileY == tilemap.tileCountY / 2) {
							tileValue = 1;
						}
					} 
					else if (tileY == 0) {
						tileValue = 2;
						if (doorDown && tileX == tilemap.tileCountX / 2) {
							tileValue = 1;
						}
					} 
					else if (tileX == tilemap.tileCountX - 1) {
						tileValue = 2;
						if (doorRight && tileY == tilemap.tileCountY / 2) {
							tileValue = 1;
						}
					}
					else if (tileY == tilemap.tileCountY - 1) {
						tileValue = 2;
						if (doorUp && tileX == tilemap.tileCountX / 2) {
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
					SetTileValue(state->worldArena, tilemap, absTileX, absTileY, absTileZ, tileValue);
					if (tileValue == 2) {
						AddWall(state, absTileX, absTileY, absTileZ);
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

	/*LowEntity* cameraEntity = GetEntity(state, state->cameraEntityIndex);
	if (cameraEntity) {*/
		/*DiffTilePosition cameraPlayerDiff = Subtract(tilemap, cameraEntity->pos, state->cameraPos);
		i32 cameraMoveX = scast(i32, (cameraPlayerDiff.dXY.X / (tilemap.tileSizeInMeters.X * scast(f32, tilemap.tileCountX) / 2.f)));
		i32 cameraMoveY = scast(i32, (cameraPlayerDiff.dXY.Y / (tilemap.tileSizeInMeters.Y * scast(f32, tilemap.tileCountY) / 2.f)));
		state->cameraPos.absX += cameraMoveX * tilemap.tileCountX;
		state->cameraPos.absY += cameraMoveY * tilemap.tileCountY;
		state->cameraPos.absZ = cameraEntity->pos.absZ;*/

		// TODO change entities to camera space
		
	//}
	for (u32 entityIndex = 1; entityIndex < state->lowEntityCount; entityIndex++) {
		MakeEntityHighFrequency(state, entityIndex);
	}

	for (u32 playerIdx = 0; playerIdx < MAX_CONTROLLERS; playerIdx++) {
		if (controllers[playerIdx].isSpaceDown && state->playerEntityIndexes[playerIdx] == 0) {
			state->playerEntityIndexes[playerIdx] = InitializePlayer(state);
		}
		LowEntity* playerLow = GetEntity(state, state->playerEntityIndexes[playerIdx]);
		if (!playerLow) {
			continue;
		}
		Assert(playerLow && playerLow->highEntityIndex > 0);
		// Shouldn't this loop be done for all the entities?
		HighEntity* playerHigh = &state->highEntities[playerLow->highEntityIndex];
		MovePlayer(state, tilemap, controllers[playerIdx], playerHigh);
	}
	
	
	

	f32 pixelsPerMeter = 42.85714f;
	//pixelsPerMeter = 3.85714f;
	V2 tileSizePixels = { tilemap.tileSizeInMeters.X * pixelsPerMeter,
						  tilemap.tileSizeInMeters.Y * pixelsPerMeter };
	V2 lowerStart = { -tilemap.tileSizeInMeters.X * pixelsPerMeter / 2.0f,
					  scast(f32, bitmap.height) };
	
	
	u32 cameraEntityRelX = 8;
	u32 cameraEntityRelY = 4;
	//if (cameraEntity) {
	//	u32 cameraEntityAbsX = cameraEntity->pos.absX; // TODO NO PLAYERABSX IN RENDERING TILEMAP!!!! ONLY CAMERA
	//	u32 cameraEntityAbsY = cameraEntity->pos.absY;
	//	cameraEntityRelX = cameraEntityAbsX % tilemap.tileCountX;
	//	cameraEntityRelY = cameraEntityAbsY % tilemap.tileCountY;
	//}
	u32 cameraRelX = state->cameraPos.absX % tilemap.tileCountX;
	u32 cameraRelY = state->cameraPos.absY % tilemap.tileCountY;
	f32 cameraFloatCameraSpaceX = (cameraRelX * tilemap.tileSizeInMeters.X + state->cameraPos.offset.X) * pixelsPerMeter;
	f32 cameraFloatCameraSpaceY = (cameraRelY * tilemap.tileSizeInMeters.Y + state->cameraPos.offset.Y) * pixelsPerMeter;
	RenderRectangle(bitmap, V2{ 0, 0 }, V2{ scast(f32, bitmap.width), scast(f32, bitmap.height) }, 1.0f, 0.f, 0.f);
	for (i32 relRow = -20; relRow < 20; relRow++) {
		for (i32 relCol = -20; relCol < 20; relCol++) {
			f32 R = 1.f;
			f32 G = 1.f;
			f32 B = 1.f;
			u32 tile = GetTileValue(
				tilemap,
				state->cameraPos.absX + relCol,
				state->cameraPos.absY + relRow,
				state->cameraPos.absZ
			);
			if (tile == 4) {
				R = 0.f;
				G = 0.f;
				B = 1.f;
			}
			else if (tile == 3) {
				R = 0.f;
				G = 1.f;
				B = 0.f;
			}
			else if (tile == 2) {
				R = 1.f;
				G = 1.f;
				B = 1.f;
			}
			else if (tile == 1) {
				R = scast(f32, ((relRow + 20) * (relCol + 20)) / 400.f) * 0.5f;
				G = 0.5f;
				B = 0.5f;
			}
			else if (tile == 0) {
				continue;
			}
			if (scast(u32, relRow) == (cameraEntityRelY - cameraRelY) && scast(u32, relCol) == (cameraEntityRelX - cameraRelX)) {
				R = 0.2f;
				G = 0.2f;
				B = 0.2f;
			}
			V2 center = { lowerStart.X + (cameraRelX + relCol) * tileSizePixels.X,
						  lowerStart.Y - (cameraRelY + relRow) * tileSizePixels.Y};
			V2 min = { center.X,
					   center.Y - tileSizePixels.Y };
			V2 max = { min.X + tileSizePixels.X,
					   center.Y };
			RenderRectangle(bitmap, min, max, R, G, B);
		}
	}



	for (u32 entityIndex = 1; entityIndex < state->highEntityCount; entityIndex++) {
		HighEntity* entity = &state->highEntities[entityIndex];
		if (!entity) {
			continue;
		}
		LowEntity* low = GetEntity(state, entity->lowEntityIndex);
		if (!low) {
			continue;
		}
#if 1
		f32 playerFloatSpaceRelToCamX = entity->pos.X * pixelsPerMeter;
		f32 playerFloatSpaceRelToCamY = entity->pos.Y * pixelsPerMeter;
		f32 playerCameraSpaceX = cameraFloatCameraSpaceX + playerFloatSpaceRelToCamX;
		f32 playerCameraSpaceY = cameraFloatCameraSpaceY + playerFloatSpaceRelToCamY;
#else
		f32 playerFloatSpaceRelToCamX = ((scast(i32, player->low->pos.absX) - scast(i32, state->cameraPos.absX)) * tilemap.tileSizeInMeters.X + (player->low->pos.offset.X - state->cameraPos.offset.X) - player->low->size.X / 2.0f) * pixelsPerMeter + tileSizePixels.X / 2.0f;
		f32 playerFloatSpaceRelToCamY = ((scast(i32, player->low->pos.absY) - scast(i32, state->cameraPos.absY)) * tilemap.tileSizeInMeters.Y + (player->low->pos.offset.Y - state->cameraPos.offset.Y)) * pixelsPerMeter + tileSizePixels.Y / 2.0f;
		f32 playerCameraSpaceX = cameraFloatCameraSpaceX + playerFloatSpaceRelToCamX;
		f32 playerCameraSpaceY = cameraFloatCameraSpaceY + playerFloatSpaceRelToCamY;
#endif
		V2 center = { lowerStart.X + playerCameraSpaceX,
					  lowerStart.Y - playerCameraSpaceY };
		V2 min = { center.X,
				   center.Y - low->size.Y * pixelsPerMeter };
		V2 max = { min.X + low->size.X * pixelsPerMeter,
				   center.Y };
		if (low->type == EntityType_Wall) {
			RenderRectangle(bitmap, min, max, 1.f, 1.f, 1.f);
		}
		else if (low->type == EntityType_Player) {
			RenderRectangle(bitmap, min, max, 0.f, 1.f, 1.f);
		}
		else {
			RenderRectangle(bitmap, min, max, 0.f, 1.f, 0.f);
		}
		

#if 0
		V2 collisionRadius = { player->low->size.X / 2.f,
							   player->low->size.Y / 4.f };
		{
			f32 debugPointX1 = lowerStart.X + (player->low->pos.absX * tilemap.tileSizeInMeters.X + player->low->pos.offset.X) * pixelsPerMeter + tileSizePixels.X / 2.0f;
			f32 debugPointX2 = debugPointX1 + 3;
			f32 debugPointY2 = lowerStart.Y - (player->low->pos.absY * tilemap.tileSizeInMeters.Y + player->low->pos.offset.Y) * pixelsPerMeter - tileSizePixels.Y / 2.0f;
			f32 debugPointY1 = debugPointY2 - 3;
			RenderRectangle(
				bitmap,
				V2{ debugPointX1 , debugPointY1 },
				V2{ debugPointX2 , debugPointY2 },
				1.f, 0.f, 0.f
			);
		}
		{
			f32 debugPointX1 = lowerStart.X + (player->pos.absX * tilemap.tileSizeInMeters.X + player->pos.offset.X - collisionRadius.X) * pixelsPerMeter + tileSizePixels.X / 2.0f;
			f32 debugPointX2 = debugPointX1 + 2 * collisionRadius.X * pixelsPerMeter;
			f32 debugPointY2 = lowerStart.Y - (player->pos.absY * tilemap.tileSizeInMeters.Y + player->pos.offset.Y - collisionRadius.Y) * pixelsPerMeter - tileSizePixels.Y / 2.0f;
			f32 debugPointY1 = debugPointY2 - 2 * collisionRadius.Y * pixelsPerMeter;
			RenderRectangle(
				bitmap,
				V2{ debugPointX1 , debugPointY1 },
				V2{ debugPointX2 , debugPointY2 },
				1.f, 0.f, 0.f
			);
		}
		RenderBitmap(
			bitmap,
			state->playerMoveAnim[player->faceDir],
			V2{ min.X , min.Y + player->size.Y * pixelsPerMeter }
		);
#endif
	}
}