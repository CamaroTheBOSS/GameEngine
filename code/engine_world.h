#pragma once
#include "engine_common.h"

struct TilePosition {
	// 28 bytes = chunk pos, 4 bytes = tile pos inside chunk
	i32 chunkX;
	i32 chunkY;
	i32 chunkZ;

	// Pos inside tile chunk in meters
	V2 offset;
};

struct LowEntityBlock {
	u32 entityIndexes[16];
	LowEntityBlock* next;
};

struct TileChunk {
	i32 chunkX;
	i32 chunkY;
	i32 chunkZ;

	LowEntityBlock* entities;
	TileChunk* next;
};

struct World {
	u32 tileCountX;
	u32 tileCountY;
	V2 tileSizeInMeters;
	V2 chunkSizeInMeters;

	TileChunk* hashTileChunks[4096];
};