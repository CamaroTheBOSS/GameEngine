#pragma once
#include "engine_common.h"

struct WorldPosition {
	// 28 bytes = chunk pos, 4 bytes = tile pos inside chunk
	i32 chunkX;
	i32 chunkY;
	i32 chunkZ;

	// Pos inside world chunk in meters
	V2 offset;
};

struct LowEntityBlock {
	u32 entityIndexes[16];
	LowEntityBlock* next;
};

struct WorldChunk {
	i32 chunkX;
	i32 chunkY;
	i32 chunkZ;

	LowEntityBlock* entities;
	WorldChunk* next;
};

struct World {
	u32 tileCountX;
	u32 tileCountY;
	V2 tileSizeInMeters;
	V2 chunkSizeInMeters;

	WorldChunk* hashWorldChunks[4096];
};