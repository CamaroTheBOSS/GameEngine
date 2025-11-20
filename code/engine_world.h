#pragma once
#include "engine_common.h"

struct TilePosition {
	// 28 bytes = chunk pos, 4 bytes = tile pos inside chunk
	u32 absX;
	u32 absY;
	u32 absZ;

	// Pos inside tile in meters
	V2 offset;
};

struct TileChunkPosition {
	// chunk index
	u32 chunkX;
	u32 chunkY;
	u32 chunkZ;

	// tile index relative to chunk
	u32 relTileX;
	u32 relTileY;
};

struct TileChunk {
	u32 chunkX;
	u32 chunkY;
	u32 chunkZ;

	u32* tiles;
	TileChunk* next;
};

struct TileMap {
	u32 chunkCountX;
	u32 chunkCountY;
	u32 chunkCountZ;
	u32 chunkDim;
	u32 chunkShift;
	u32 chunkMask;

	u32 tileCountX;
	u32 tileCountY;
	V2 tileSizeInMeters;

	TileChunk* hashTileChunks[4096];
};