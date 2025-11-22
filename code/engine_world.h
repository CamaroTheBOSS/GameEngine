#pragma once
#include "engine_common.h"

struct TilePosition {
	// 28 bytes = chunk pos, 4 bytes = tile pos inside chunk
	i32 absX;
	i32 absY;
	i32 absZ;

	// Pos inside tile in meters
	V2 offset;
};

struct TileChunkPosition {
	// chunk index
	i32 chunkX;
	i32 chunkY;
	i32 chunkZ;

	// offset from center of a chunk
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

struct TileMap {
	u32 chunkDim;
	u32 chunkShift;
	u32 chunkMask;

	u32 tileCountX;
	u32 tileCountY;
	V2 tileSizeInMeters;

	TileChunk* hashTileChunks[4096];
};