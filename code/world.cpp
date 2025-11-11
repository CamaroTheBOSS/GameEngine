#include "main_header.h"

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
u32 GetTileValue(TileMap& tilemap, u32 absX, u32 absY, u32 absZ) {
	TileChunkPosition chunkPos = GetTileChunkPosition(tilemap, absX, absY, absZ);
	TileChunk* chunk = GetTileChunk(tilemap, chunkPos.chunkX, chunkPos.chunkY, chunkPos.chunkZ);
	u32 value = GetTileValue(tilemap, chunk, chunkPos.relTileX, chunkPos.relTileY);
	return value;
}

inline internal
u32 GetTileValue(TileMap& tilemap, TilePosition& position) {
	return GetTileValue(tilemap, position.absX, position.absY, position.absZ);
}

inline internal
void SetTileValue(TileMap& tilemap, TileChunk* chunk, u32 relX, u32 relY, u32 tileValue) {
	if (chunk && chunk->tiles && relY < tilemap.chunkDim && relX < tilemap.chunkDim) {
		chunk->tiles[relY * tilemap.chunkDim + relX] = tileValue;
	}
}

inline internal
void SetTileValue(MemoryArena& arena, TileMap& tilemap, u32 absX, u32 absY, u32 absZ, u32 tileValue) {
	TileChunkPosition chunkPos = GetTileChunkPosition(tilemap, absX, absY, absZ);
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
bool IsWorldPointEmpty(TileMap& tilemap, u32 absX, u32 absY, u32 absZ) {
	TileChunkPosition chunkPos = GetTileChunkPosition(tilemap, absX, absY, absZ);
	TileChunk* chunk = GetTileChunk(tilemap, chunkPos.chunkX, chunkPos.chunkY, chunkPos.chunkZ);
	u32 tileValue = GetTileValue(tilemap, chunk, chunkPos.relTileX, chunkPos.relTileY);
	bool isEmpty = tileValue == 1 || tileValue == 3 || tileValue == 4;
	return isEmpty;
}

inline internal
bool IsWorldPointEmpty(TileMap& tilemap, TilePosition& position) {
	return IsWorldPointEmpty(tilemap, position.absX, position.absY, position.absZ);
}

inline internal
bool AreOnTheSameTile(TilePosition& first, TilePosition& second) {
	bool result = first.absX == second.absX &&
		first.absY == second.absY &&
		first.absZ == second.absZ;
	return result;
}

inline internal
void FixTilePosition(TileMap& tilemap, TilePosition& position) {
	i32 offsetX = RoundF32ToI32(position.X / tilemap.tileSizeInMetersX);
	i32 offsetY = RoundF32ToI32(position.Y / tilemap.tileSizeInMetersY);
	position.absX += offsetX;
	position.absY += offsetY;
	position.X -= offsetX * tilemap.tileSizeInMetersX;
	position.Y -= offsetY * tilemap.tileSizeInMetersY;
	Assert(position.X >= -tilemap.tileSizeInMetersX / 2.0f);
	Assert(position.Y >= -tilemap.tileSizeInMetersY / 2.0f);
	Assert(position.X <= tilemap.tileSizeInMetersX / 2.0f);
	Assert(position.Y <= tilemap.tileSizeInMetersY / 2.0f);
}

struct DiffTilePosition {
	f32 dX;
	f32 dY;
	f32 dZ;
};

inline internal
DiffTilePosition Subtract(TileMap& tilemap, TilePosition& first, TilePosition& second) {
	DiffTilePosition diff = {};
	f32 firstMetersX = scast(f32, first.absX * tilemap.tileSizeInMetersX) + first.X;
	f32 firstMetersY = scast(f32, first.absY * tilemap.tileSizeInMetersY) + first.Y;
	f32 secondMetersX = scast(f32, second.absX * tilemap.tileSizeInMetersX) + second.X;
	f32 secondMetersY = scast(f32, second.absY * tilemap.tileSizeInMetersY) + second.Y;
	diff.dX = firstMetersX - secondMetersX;
	diff.dY = firstMetersY - secondMetersY;
	diff.dZ = scast(f32, first.absZ) - scast(f32, second.absZ);
	return diff;
}
