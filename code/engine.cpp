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
void RenderRectangle(BitmapData& bitmap, f32 X1, f32 Y1, f32 X2, f32 Y2, f32 R, f32 G, f32 B) {
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
void RenderBitmap(BitmapData& screenBitmap, LoadedBitmap& loadedBitmap, f32 X1, f32 Y1) {
	i32 minX = RoundF32ToI32(X1) - loadedBitmap.alignX;
	i32 maxX = minX + loadedBitmap.width;
	i32 minY = RoundF32ToI32(Y1) - loadedBitmap.alignY;
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

extern "C" GAME_MAIN_LOOP_FRAME(GameMainLoopFrame) {
	ProgramState* state = ptrcast(ProgramState, memory.permanentMemory);
	TileMap& tilemap = state->world.tilemap;
	if (!state->isInitialized) {
		state->playerPos.absX = 3;
		state->playerPos.absY = 3;
		state->playerPos.absZ = 0;
		state->playerPos.offset.X = 0;
		state->playerPos.offset.Y = 0;

		state->worldArena.data = ptrcast(u8, memory.permanentMemory) + sizeof(ProgramState);
		state->worldArena.capacity = memory.permanentMemorySize - sizeof(ProgramState);
		state->worldArena.used = 0;

		tilemap.tileCountX = 17;
		tilemap.tileCountY = 9;
		tilemap.tileSizeInMeters.X = 1.4f;
		tilemap.tileSizeInMeters.Y = tilemap.tileSizeInMeters.X;
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
		state->cameraPos.offset.X = 0;
		state->cameraPos.offset.Y = 0;

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
		for (u32 screenIndex = 0; screenIndex < 20; screenIndex++) {
			randomNIdx = (randomNIdx + 1) % ArrayCount(randomNumbers, u32);
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
		state->playerBitmaps[0] = LoadBmpFile(memory.debugReadEntireFile, "test/hero-right.bmp");
		state->playerBitmaps[0].alignX = 0;
		state->playerBitmaps[0].alignY = 55;
		
		state->playerBitmaps[1] = LoadBmpFile(memory.debugReadEntireFile, "test/hero-left.bmp");
		state->playerBitmaps[1].alignX = 0;
		state->playerBitmaps[1].alignY = 55;

		state->playerBitmaps[2] = LoadBmpFile(memory.debugReadEntireFile, "test/hero-up.bmp");
		state->playerBitmaps[2].alignX = 0;
		state->playerBitmaps[2].alignY = 55;

		state->playerBitmaps[3] = LoadBmpFile(memory.debugReadEntireFile, "test/hero-down.bmp");
		state->playerBitmaps[3].alignX = 0;
		state->playerBitmaps[3].alignY = 55;

		state->playerFaceDirection = 0;

		state->isInitialized = true;
	}

	V2 movement = {};
	f32 speed = 1.0f;
	if (inputData.isADown) {
		state->playerFaceDirection = 1;
		movement.X -= 5.f;
	}
	if (inputData.isWDown) {
		state->playerFaceDirection = 2;
		movement.Y += 5.f;
	}
	if (inputData.isSDown) {
		state->playerFaceDirection = 3;
		movement.Y -= 5.f;
	}
	if (inputData.isDDown) {
		state->playerFaceDirection = 0;
		movement.X += 5.f;
	} if (inputData.isSpaceDown) {
		speed = 4.0f;
	}
	if (inputData.isEscDown) {
		state->playerPos.offset.X = 0.f;
		state->playerPos.offset.Y = 0.f;
	}
	movement *= speed * inputData.dtFrame;
	
	f32 playerWidth = tilemap.tileSizeInMeters.X * 0.7f;
	f32 playerHeight = tilemap.tileSizeInMeters.Y * 0.9f;
	TilePosition nextPlayerPosition = state->playerPos;
	nextPlayerPosition.offset.X += movement.X;
	nextPlayerPosition.offset.Y += movement.Y;
	FixTilePosition(tilemap, nextPlayerPosition);

	TilePosition playerNextTilePosLeft = nextPlayerPosition;
	playerNextTilePosLeft.offset.X -= playerWidth / 2.0f;
	FixTilePosition(tilemap, playerNextTilePosLeft);

	TilePosition playerNextTilePosRight = nextPlayerPosition;
	playerNextTilePosRight.offset.X += playerWidth / 2.0f;
	FixTilePosition(tilemap, playerNextTilePosRight);

	if (IsWorldPointEmpty(tilemap, nextPlayerPosition) &&
		IsWorldPointEmpty(tilemap, playerNextTilePosLeft) &&
		IsWorldPointEmpty(tilemap, playerNextTilePosRight)) 
	{
		if (!AreOnTheSameTile(state->playerPos, nextPlayerPosition)) {
			u32 tileValue = GetTileValue(tilemap, nextPlayerPosition);
			if (tileValue == 3) {
				state->playerPos.absZ = 1;
			}
			else if (tileValue == 4) {
				state->playerPos.absZ = 0;
			}
		}
		state->playerPos.offset.X += movement.X;
		state->playerPos.offset.Y += movement.Y;
		FixTilePosition(tilemap, state->playerPos);
	}

	DiffTilePosition cameraPlayerDiff = Subtract(tilemap, state->playerPos, state->cameraPos);
	i32 cameraMoveX = scast(i32, (cameraPlayerDiff.dX / (tilemap.tileSizeInMeters.X * scast(f32, tilemap.tileCountX) / 2.f)));
	i32 cameraMoveY = scast(i32, (cameraPlayerDiff.dY / (tilemap.tileSizeInMeters.Y * scast(f32, tilemap.tileCountY) / 2.f)));
	state->cameraPos.absX += cameraMoveX * tilemap.tileCountX;
	state->cameraPos.absY += cameraMoveY * tilemap.tileCountY;
	state->cameraPos.absZ = state->playerPos.absZ;

	f32 pixelsPerMeter = 42.85714f;
	//f32 pixelsPerMeter = 3.85714f;
	f32 tileSizePixelsX = tilemap.tileSizeInMeters.X * pixelsPerMeter;
	f32 tileSizePixelsY = tilemap.tileSizeInMeters.Y * pixelsPerMeter;
	f32 lowerStartX = -tilemap.tileSizeInMeters.X * pixelsPerMeter / 2.0f;
	f32 lowerStartY = scast(f32, bitmap.height);
	u32 playerAbsX = state->playerPos.absX;
	u32 playerAbsY = state->playerPos.absY;
	u32 playerRelX = playerAbsX % tilemap.tileCountX;
	u32 playerRelY = playerAbsY % tilemap.tileCountY;
	u32 cameraRelX = state->cameraPos.absX % tilemap.tileCountX;
	u32 cameraRelY = state->cameraPos.absY % tilemap.tileCountY;
	
	RenderRectangle(bitmap, 0.f, 0.f, scast(f32, bitmap.width), scast(f32, bitmap.height), 1.0f, 0.f, 0.f);
	for (i32 relRow = -20; relRow < 20; relRow++) {
		for (i32 relCol = -20; relCol < 20; relCol++) {
			f32 R = 1.f;
			f32 G = 1.f;
			f32 B = 1.f;
			u32 playerMapX = (playerAbsX / tilemap.tileCountX) * tilemap.tileCountX;
			u32 playerMapY = (playerAbsY / tilemap.tileCountY) * tilemap.tileCountY;
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
			if (scast(u32, relRow) == (playerRelY - cameraRelY) && scast(u32, relCol) == (playerRelX - cameraRelX)) {
				R = 0.2f;
				G = 0.2f;
				B = 0.2f;
			}
			f32 cenX = lowerStartX + (cameraRelX + relCol) * tileSizePixelsX;
			f32 cenY = lowerStartY - (cameraRelY + relRow) * tileSizePixelsY;
			f32 minX = cenX;
			f32 maxX = minX + tileSizePixelsX;
			f32 maxY = cenY;
			f32 minY = maxY - tileSizePixelsY;
			RenderRectangle(bitmap, minX, minY, maxX, maxY, R, G, B);
		}
	}
	f32 playerPixMinX = lowerStartX + (playerRelX * tilemap.tileSizeInMeters.X + state->playerPos.offset.X - playerWidth / 2.0f) * pixelsPerMeter + tileSizePixelsX / 2.0f;
	f32 playerPixMaxX = playerPixMinX + pixelsPerMeter * playerWidth;
	f32 playerPixMaxY = lowerStartY - (playerRelY * tilemap.tileSizeInMeters.Y + state->playerPos.offset.Y) * pixelsPerMeter - tileSizePixelsY / 2.0f;
	f32 playerPixMinY = playerPixMaxY - pixelsPerMeter * playerHeight;
	RenderRectangle(
		bitmap,
		playerPixMinX,
		playerPixMinY,
		playerPixMaxX,
		playerPixMaxY,
		0.f, 1.f, 1.f
	);

	f32 debugPointX1 = lowerStartX + (playerAbsX * tilemap.tileSizeInMeters.X + state->playerPos.offset.X) * pixelsPerMeter + tileSizePixelsX / 2.0f;
	f32 debugPointX2 = debugPointX1 + 3;
	f32 debugPointY2 = lowerStartY - (playerAbsY * tilemap.tileSizeInMeters.Y + state->playerPos.offset.Y) * pixelsPerMeter - tileSizePixelsY / 2.0f;
	f32 debugPointY1 = debugPointY2 - 3;
	RenderRectangle(
		bitmap,
		debugPointX1,
		debugPointY1,
		debugPointX2,
		debugPointY2,
		1.f, 0.f, 0.f
	);

	/*RenderBitmap(
		bitmap, 
		state->playerBitmaps[state->playerFaceDirection],
		playerPixMinX, 
		playerPixMaxY
	);*/
}