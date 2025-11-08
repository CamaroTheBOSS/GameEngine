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

internal 
void RenderRectangle(BitmapData& bitmap, f32 X1, f32 Y1, f32 X2, f32 Y2, f32 R, f32 G, f32 B) {
	Assert(X1 < X2);
	Assert(Y1 < Y2);
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

u32 GetTileValue(World& world, u32 tileMapX, u32 tileMapY, u32 tileX, u32 tileY) {
	Assert(tileX < world.sizeX);
	Assert(tileY < world.sizeY);
	u32* tiles = world.tileMap->tiles;
	// TODO tileY * 34 = tileY * chunkXSize i guess
	return tiles[tileMapY * 34 * world.sizeY + tileMapX * world.sizeX + tileY * 34 + tileX];
}

bool IsTileMapPointEmpty(World& world, TilePosition& position) {
	bool isEmpty = GetTileValue(
		world, 
		position.tileMapX, position.tileMapY, 
		position.tileX, position.tileY
	) == 0;
	return isEmpty;
}

internal
void FixTilePosition(World& world, TilePosition& position) {
	f32 tileCoordX = position.X / scast(f32, world.widthMeters);
	f32 tileCoordY = position.Y / scast(f32, world.heightMeters);
	i32 tileAddX = FloorF32ToI32(tileCoordX);
	i32 tileAddY = FloorF32ToI32(tileCoordY);
	position.X -= scast(f32, tileAddX * scast(f32, world.widthMeters));
	position.Y -= scast(f32, tileAddY * scast(f32, world.heightMeters));
	position.tileX += tileAddX;
	position.tileY += tileAddY;
	i32 tileMapAddX = FloorF32ToI32(position.tileX / scast(f32, world.sizeX));
	i32 tileMapAddY = FloorF32ToI32(position.tileY / scast(f32, world.sizeY));
	position.tileX -= tileMapAddX * world.sizeX;
	position.tileY -= tileMapAddY * world.sizeY;
	position.tileMapX += tileMapAddX;
	position.tileMapY += tileMapAddY;
	Assert(position.tileX < scast(i32, world.sizeX));
	Assert(position.tileY < scast(i32, world.sizeY));
	Assert(position.X >= 0);
	Assert(position.X < world.widthMeters);
	Assert(position.Y >= 0);
	Assert(position.Y < world.heightMeters);
}

TilePosition GetTilePosition(World& world, f32 pixelX, f32 pixelY) {
	f32 rawNormalizedPosX = (pixelX - scast(f32, world.offsetPixelsX)) / world.pixelsPerMeter;
	f32 rawNormalizedPosY = (pixelY - scast(f32, world.offsetPixelsY)) / world.pixelsPerMeter;
	f32 rawTilePosX = rawNormalizedPosX / scast(f32, world.widthMeters);
	f32 rawTilePosY = rawNormalizedPosY / scast(f32, world.heightMeters);
	i32 tilePosX = FloorF32ToI32(rawTilePosX);
	i32 tilePosY = FloorF32ToI32(rawTilePosY);
	TilePosition tilePos;
	tilePos.tileMapX = tilePosX / world.sizeX;
	tilePos.tileMapY = tilePosY / world.sizeY;
	tilePos.tileX = tilePosX - tilePos.tileMapX * world.sizeX;
	tilePos.tileY = tilePosY - tilePos.tileMapY * world.sizeY;
	tilePos.X = rawNormalizedPosX - tilePosX * world.widthMeters;
	tilePos.Y = rawNormalizedPosY - tilePosY * world.heightMeters;
	//Assert(tilePos.tileMapX < world.allTileMapsSizeX);
	//Assert(tilePos.tileMapY < world.allTileMapsSizeY);
	Assert(tilePos.tileX < scast(i32, world.sizeX));
	Assert(tilePos.tileY < scast(i32, world.sizeY));
	Assert(tilePos.X >= 0.f);
	Assert(tilePos.X < world.widthMeters);
	Assert(tilePos.Y >= 0.f);
	Assert(tilePos.Y < world.heightMeters);
	return tilePos;
}

extern "C" GAME_MAIN_LOOP_FRAME(GameMainLoopFrame) {
	ProgramState* state = ptrcast(ProgramState, memory.permanentMemory);
	if (!state->isInitialized) {
		state->playerPos.tileMapX = 0;
		state->playerPos.tileMapY = 0;
		state->playerPos.tileX = 3;
		state->playerPos.tileY = 3;
		state->playerPos.X = 0;
		state->playerPos.Y = 0;
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
	u32 tileMap[18][34] = {
		{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
		{1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	};
	
	World world = {};
	world.sizeX = 17;
	world.sizeY = 9;
	world.widthPixels = 60;
	world.heightPixels = world.widthPixels;
	world.widthMeters = 1.4f;
	world.heightMeters = world.widthMeters;
	world.pixelsPerMeter = world.widthPixels / world.widthMeters;
	world.offsetPixelsX = -scast(i32, scast(f32, world.widthPixels) / 2.0f);
	world.offsetPixelsY = 0;
	world.chunkSizeX = 256;
	world.chunkSizeY = 256;
	TileMap tilemap;
	tilemap.tiles = ptrcast(u32, tileMap);
	world.tileMap = &tilemap;

	f32 playerWidth = scast(f32, world.widthMeters) * 0.7f;
	f32 playerHeight = scast(f32, world.heightMeters) * 0.9f;
	TilePosition nextTilePosition = state->playerPos;
	nextTilePosition.X += moveX;
	nextTilePosition.Y += moveY;
	TilePosition playerNextTilePosLeft = nextTilePosition;
	playerNextTilePosLeft.X -= playerWidth / 2.0f;
	FixTilePosition(world, playerNextTilePosLeft);
	TilePosition playerNextTilePosRight = nextTilePosition;
	playerNextTilePosRight.X += playerWidth / 2.0f;
	FixTilePosition(world, playerNextTilePosRight);

	
	if (IsTileMapPointEmpty(world, playerNextTilePosLeft) &&
		IsTileMapPointEmpty(world, playerNextTilePosRight)) {
		state->playerPos.X += moveX;
		state->playerPos.Y += moveY;
		FixTilePosition(world, state->playerPos);
	}

	RenderRectangle(bitmap, 0.f, 0.f, scast(f32, bitmap.width), scast(f32, bitmap.height), 1.0f, 0.f, 0.f);
	for (u32 row = 0; row < world.sizeY; row++) {
		for (u32 col = 0; col < world.sizeX; col++) {
			f32 gray = 1.0f;
			u32 tile = GetTileValue(world, state->playerPos.tileMapX, 
				state->playerPos.tileMapY, col, row);
			if (tile == 0) {
				gray = 0.5f;
			}
			else if (tile == 1) {
				gray = 1.0f;
			}
			if (row == scast(u32, state->playerPos.tileY) && col == scast(u32, state->playerPos.tileX)) {
				gray = 0.2f;
			}
			f32 x1 = scast(f32, world.offsetPixelsX) + scast(f32, col * world.widthPixels);
			f32 y1 = bitmap.height + scast(f32, world.offsetPixelsY) - scast(f32, (row + 1) * world.heightPixels);
			f32 x2 = x1 + scast(f32, world.widthPixels);
			f32 y2 = y1 + scast(f32, world.heightPixels);
			RenderRectangle(bitmap, x1, y1, x2, y2, gray, gray, gray);
		}
	}

	f32 playerPixX = (state->playerPos.tileX * world.widthMeters + state->playerPos.X - playerWidth / 2.0f) * world.pixelsPerMeter + world.offsetPixelsX;
	f32 playerPixY = bitmap.height - (state->playerPos.tileY * world.heightMeters + state->playerPos.Y + playerHeight) * world.pixelsPerMeter + world.offsetPixelsY;
	RenderRectangle(
		bitmap,
		playerPixX,
		playerPixY,
		playerPixX + world.pixelsPerMeter * playerWidth,
		playerPixY + world.pixelsPerMeter * playerHeight,
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