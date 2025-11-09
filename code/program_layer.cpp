#include "main_header.h"

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

#include "math.h"

internal
i32 RoundF32ToI32(f32 value) {
	return scast(i32, roundf(value));
}

internal
i32 FloorF32ToI32(f32 value) {
	return scast(i32, floorf(value));
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
void RenderRectangle(BitmapData& bitmap, f32 X1, f32 Y1, f32 X2, f32 Y2, f32 R, f32 G, f32 B) {
	//Assert(X1 < X2);
	//Assert(Y1 < Y2);
	i32 minX = RoundF32ToI32(X1);
	i32 maxX = RoundF32ToI32(X2);
	i32 minY = RoundF32ToI32(Y1);
	i32 maxY = RoundF32ToI32(Y2);
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

inline internal
TileChunkPosition GetTileChunkPosition(TileMap& tilemap, u32 absX, u32 absY, u32 absZ) {
	TileChunkPosition chunkPos;
	chunkPos.chunkX = absX >> tilemap.chunkShift;
	chunkPos.chunkY = absY >> tilemap.chunkShift;
	chunkPos.chunkZ = absZ;
	chunkPos.relTileX = absX & tilemap.chunkMask;
	chunkPos.relTileY = absY & tilemap.chunkMask;
	// Make sure we have enough bits to store it in absX;
	return chunkPos;
}

inline internal
TileChunk* GetTileChunk(TileMap& tilemap, u32 chunkX, u32 chunkY, u32 chunkZ) {
	TileChunk* tileChunk = 0;
	if (chunkX >= 0 && chunkX < tilemap.chunkCountX &&
		chunkY >= 0 && chunkY < tilemap.chunkCountY &&
		chunkZ >= 0 && chunkZ < tilemap.chunkCountZ) 
	{
		tileChunk = &tilemap.tileChunks[chunkZ * tilemap.chunkCountY * tilemap.chunkCountX + 
										chunkY * tilemap.chunkCountX + chunkX];
	}
	return tileChunk;
}

inline internal
u32 GetTileValue(TileMap& tilemap, TileChunk* chunk, u32 relX, u32 relY) {
	if (chunk && chunk->tiles && relY < tilemap.chunkDim && relX < tilemap.chunkDim) {
		return chunk->tiles[relY * tilemap.chunkDim + relX];
	}
	return 0;
}

inline internal
u32 GetTileValue(TileMap& tilemap, u32 absX, u32 absY) {
	TileChunkPosition chunkPos = GetTileChunkPosition(tilemap, absX, absY, 0);
	TileChunk* chunk = GetTileChunk(tilemap, chunkPos.chunkX, chunkPos.chunkY, chunkPos.chunkZ);
	u32 value = GetTileValue(tilemap, chunk, chunkPos.relTileX, chunkPos.relTileY);
	return value;
}

inline internal
void SetTileValue(TileMap& tilemap, TileChunk* chunk, u32 relX, u32 relY, u32 tileValue) {
	if (chunk && chunk->tiles && relY < tilemap.chunkDim && relX < tilemap.chunkDim) {
		chunk->tiles[relY * tilemap.chunkDim + relX] = tileValue;
	}
}

inline internal
void SetTileValue(MemoryArena& arena, TileMap& tilemap, u32 absX, u32 absY, u32 tileValue) {
	TileChunkPosition chunkPos = GetTileChunkPosition(tilemap, absX, absY, 0);
	TileChunk* chunk = GetTileChunk(tilemap, chunkPos.chunkX, chunkPos.chunkY, chunkPos.chunkZ);
	if (!chunk->tiles) {
		u32 tileChunkSize = tilemap.chunkDim * tilemap.chunkDim;
		chunk->tiles = ptrcast(u32, PushArray(arena, tileChunkSize, u32));
		for (u32 tileIndex = 0; tileIndex < tileChunkSize; tileIndex++) {
			chunk->tiles[tileIndex] = 1;
		}
	}
	SetTileValue(tilemap, chunk, chunkPos.relTileX, chunkPos.relTileY, tileValue);
}

inline internal
bool IsWorldPointEmpty(TileMap& tilemap, u32 absX, u32 absY) {
	TileChunkPosition chunkPos = GetTileChunkPosition(tilemap, absX, absY, 0);
	TileChunk* chunk = GetTileChunk(tilemap, chunkPos.chunkX, chunkPos.chunkY, chunkPos.chunkZ);
	bool isEmpty = GetTileValue(tilemap, chunk, chunkPos.relTileX, chunkPos.relTileY) == 1;
	return isEmpty;
}

inline internal
void FixTilePosition(TileMap& tilemap, TilePosition& position) {
	i32 offsetX = FloorF32ToI32(position.X / tilemap.widthMeters);
	i32 offsetY = FloorF32ToI32(position.Y / tilemap.heightMeters);
	position.absX += offsetX;
	position.absY += offsetY;
	position.X -= offsetX * tilemap.widthMeters;
	position.Y -= offsetY * tilemap.heightMeters;
	Assert(position.X >= 0);
	Assert(position.Y >= 0);
	Assert(position.X < tilemap.widthMeters);
	Assert(position.Y < tilemap.heightMeters);
}

extern "C" GAME_MAIN_LOOP_FRAME(GameMainLoopFrame) {
	ProgramState* state = ptrcast(ProgramState, memory.permanentMemory);
	if (!state->isInitialized) {
		state->playerPos.absX = 3;
		state->playerPos.absY = 3;
		state->playerPos.X = 0;
		state->playerPos.Y = 0;

		state->worldArena.data = ptrcast(u8, memory.permanentMemory) + sizeof(ProgramState);
		state->worldArena.capacity = memory.permanentMemorySize - sizeof(ProgramState);
		state->worldArena.used = 0;

		state->world.tilemap.tileCountX = 17;
		state->world.tilemap.tileCountY = 9;
		state->world.tilemap.widthMeters = 1.4f;
		state->world.tilemap.heightMeters = state->world.tilemap.widthMeters;
		state->world.tilemap.chunkCountX = 32;
		state->world.tilemap.chunkCountY = 32;
		state->world.tilemap.chunkCountZ = 1;
		state->world.tilemap.chunkShift = 4;
		state->world.tilemap.chunkMask = (1 << state->world.tilemap.chunkShift) - 1;
		state->world.tilemap.chunkDim = (1 << state->world.tilemap.chunkShift);
		state->world.tilemap.tileChunks = ptrcast(TileChunk, PushArray(
			state->worldArena,
			state->world.tilemap.chunkCountX * state->world.tilemap.chunkCountY,
			TileChunk
		));
		//TODO central point is 0,0 at tile
		//TODO pixelsPerMeters as world property?

		/*for (u32 chunkY = 0; chunkY < state->world.tilemap.chunkCountY; chunkY++) {
			for (u32 chunkX = 0; chunkX < state->world.tilemap.chunkCountX; chunkX++) {
				state->world.tilemap.tileChunks[chunkY * state->world.tilemap.chunkCountX + chunkX].tiles =
					ptrcast(u32, PushArray(
						state->worldArena, 
						state->world.tilemap.chunkDim * state->world.tilemap.chunkDim, 
						u32
					));
			}
		}*/

		bool doorLeft = false;
		bool doorRight = false;
		bool doorUp = false;
		bool doorDown = false;
		u32 screenX = 0;
		u32 screenY = 0;
		u32 randomNIdx = 0;
		for (u32 screenIndex = 0; screenIndex < 10; screenIndex++) {
			randomNIdx = (randomNIdx + 1) % ArrayCount(randomNumbers, u32);
			u32 randomNumber = randomNumbers[randomNIdx];
			if (randomNumber % 2) {
				doorRight = true;
			}
			else {
				doorUp = true;
			}

			for (u32 tileY = 0; tileY < state->world.tilemap.tileCountY; tileY++) {
				for (u32 tileX = 0; tileX < state->world.tilemap.tileCountX; tileX++) {
					u32 absTileX = screenX * state->world.tilemap.tileCountX + tileX;
					u32 absTileY = screenY * state->world.tilemap.tileCountY + tileY;

					u32 tileValue = 1;
					if (tileX == 0) {
						tileValue = 2;
						if (doorLeft && tileY == state->world.tilemap.tileCountY / 2) {
							tileValue = 1;
						}
					} 
					else if (tileY == 0) {
						tileValue = 2;
						if (doorDown && tileX == state->world.tilemap.tileCountX / 2) {
							tileValue = 1;
						}
					} 
					else if (tileX == state->world.tilemap.tileCountX - 1) {
						tileValue = 2;
						if (doorRight && tileY == state->world.tilemap.tileCountY / 2) {
							tileValue = 1;
						}
					}
					else if (tileY == state->world.tilemap.tileCountY - 1) {
						tileValue = 2;
						if (doorUp && tileX == state->world.tilemap.tileCountX / 2) {
							tileValue = 1;
						}
					}
					SetTileValue(state->worldArena, state->world.tilemap, absTileX, absTileY, tileValue);
				}
			}
			doorLeft = doorRight;
			doorDown = doorUp;
			doorUp = false;
			doorRight = false;
			if (randomNumber % 2) {
				screenX++;
			}
			else {
				screenY++;
			}
		}
		state->isInitialized = true;
	}

	f32 moveX = 0.f;
	f32 moveY = 0.f;
	if (inputData.isADown) {
		moveX -= 5.f;
	}
	if (inputData.isWDown) {
		moveY += 5.f;
	}
	if (inputData.isSDown) {
		moveY -= 5.f;
	}
	if (inputData.isDDown) {
		moveX += 5.f;
	}
	moveX *= inputData.dtFrame;
	moveY *= inputData.dtFrame;
	
	f32 playerWidth = state->world.tilemap.widthMeters * 0.7f;
	f32 playerHeight = state->world.tilemap.heightMeters * 0.9f;
	TilePosition nextTilePosition = state->playerPos;
	nextTilePosition.X += moveX;
	nextTilePosition.Y += moveY;
	TilePosition playerNextTilePosLeft = nextTilePosition;
	playerNextTilePosLeft.X -= playerWidth / 2.0f;
	FixTilePosition(state->world.tilemap, playerNextTilePosLeft);
	TilePosition playerNextTilePosRight = nextTilePosition;
	playerNextTilePosRight.X += playerWidth / 2.0f;
	FixTilePosition(state->world.tilemap, playerNextTilePosRight);

	if (IsWorldPointEmpty(state->world.tilemap, playerNextTilePosLeft.absX, playerNextTilePosLeft.absY) &&
		IsWorldPointEmpty(state->world.tilemap, playerNextTilePosRight.absX, playerNextTilePosRight.absY)) {
		state->playerPos.X += moveX;
		state->playerPos.Y += moveY;
		FixTilePosition(state->world.tilemap, state->playerPos);
	}
	//f32 pixelsPerMeter = 42.85714f;
	f32 pixelsPerMeter = 6.85714f;
	f32 tilemapOffsetX = -state->world.tilemap.widthMeters * pixelsPerMeter / 2.0f;
	f32 tilemapOffsetY = 0.f;
	u32 playerAbsX = state->playerPos.absX;
	u32 playerAbsY = state->playerPos.absY;
	u32 playerRelX = playerAbsX % state->world.tilemap.tileCountX;
	u32 playerRelY = playerAbsY % state->world.tilemap.tileCountY;
	RenderRectangle(bitmap, 0.f, 0.f, scast(f32, bitmap.width), scast(f32, bitmap.height), 1.0f, 0.f, 0.f);
	for (i32 row = 0; row < 256; row++) {
		for (i32 col = 0; col < 256; col++) {
			f32 gray = 1.0f;
			u32 playerMapX = (playerAbsX / state->world.tilemap.tileCountX) * state->world.tilemap.tileCountX;
			u32 playerMapY = (playerAbsY / state->world.tilemap.tileCountY) * state->world.tilemap.tileCountY;
			u32 tile = GetTileValue(
				state->world.tilemap, 
				playerMapX + scast(u32, col),
				playerMapY + scast(u32, row)
			);
			if (tile == 2) {
				gray = 1.0f;
			}
			else if (tile == 1) {
				gray = 0.5f;
			}
			else if (tile == 0) {
				continue;
			}
			if (scast(u32, row) == playerRelY && scast(u32, col) == playerRelX) {
				gray = 0.2f;
			}
			f32 x1 = tilemapOffsetX + col * state->world.tilemap.widthMeters * pixelsPerMeter;
			f32 y1 = bitmap.height + tilemapOffsetY - (row + 1) * state->world.tilemap.heightMeters * pixelsPerMeter;
			f32 x2 = x1 + state->world.tilemap.widthMeters * pixelsPerMeter;
			f32 y2 = y1 + state->world.tilemap.heightMeters * pixelsPerMeter;
			RenderRectangle(bitmap, x1, y1, x2, y2, gray, gray, gray);
		}
	}
	f32 playerPixX = (playerRelX * state->world.tilemap.widthMeters + state->playerPos.X - playerWidth / 2.0f) * pixelsPerMeter + tilemapOffsetX;
	f32 playerPixY = bitmap.height - (playerRelY * state->world.tilemap.heightMeters + state->playerPos.Y + playerHeight) * pixelsPerMeter + tilemapOffsetY;
	RenderRectangle(
		bitmap,
		playerPixX,
		playerPixY,
		playerPixX + pixelsPerMeter * playerWidth,
		playerPixY + pixelsPerMeter * playerHeight,
		0.f, 1.f, 1.f
	);

#if 0
	PixelPosition playerNextPixelPosLeft = GetPixelPosition(world, playerNextTilePosLeft);
	PixelPosition playerNextPixelPosRight = GetPixelPosition(world, playerNextTilePosRight);
	RenderRectangle(
		bitmap,
		playerPixPos.x,
		playerPixPos.y,
		playerPixPos.x + 3,
		playerPixPos.y + 3,
		1.f, 0.f, 0.f
	);
	RenderRectangle(
		bitmap,
		playerCurrentPixelPos.x,
		playerCurrentPixelPos.y,
		playerCurrentPixelPos.x + 3,
		playerCurrentPixelPos.y + 3,
		1.f, 0.f, 1.f
	);

	RenderRectangle(
		bitmap,
		playerNextPixelPosLeft.x,
		playerNextPixelPosLeft.y,
		playerNextPixelPosLeft.x + 3,
		playerNextPixelPosLeft.y + 3,
		1.f, 1.f, 0.f
	);
	RenderRectangle(
		bitmap,
		playerNextPixelPosRight.x,
		playerNextPixelPosRight.y,
		playerNextPixelPosRight.x + 3,
		playerNextPixelPosRight.y + 3,
		1.f, 1.f, 0.f
	);
#endif
}