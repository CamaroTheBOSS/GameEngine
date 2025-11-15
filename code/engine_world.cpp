#include "engine_world.h"

inline
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

inline
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

inline
u32 GetTileValue(TileMap& tilemap, TileChunk* chunk, u32 relX, u32 relY) {
	if (chunk && chunk->tiles && relY < tilemap.chunkDim && relX < tilemap.chunkDim) {
		return chunk->tiles[relY * tilemap.chunkDim + relX];
	}
	return 0;
}

inline
u32 GetTileValue(TileMap& tilemap, u32 absX, u32 absY, u32 absZ) {
	TileChunkPosition chunkPos = GetTileChunkPosition(tilemap, absX, absY, absZ);
	TileChunk* chunk = GetTileChunk(tilemap, chunkPos.chunkX, chunkPos.chunkY, chunkPos.chunkZ);
	u32 value = GetTileValue(tilemap, chunk, chunkPos.relTileX, chunkPos.relTileY);
	return value;
}

inline
u32 GetTileValue(TileMap& tilemap, TilePosition& position) {
	return GetTileValue(tilemap, position.absX, position.absY, position.absZ);
}

inline
void SetTileValue(TileMap& tilemap, TileChunk* chunk, u32 relX, u32 relY, u32 tileValue) {
	if (chunk && chunk->tiles && relY < tilemap.chunkDim && relX < tilemap.chunkDim) {
		chunk->tiles[relY * tilemap.chunkDim + relX] = tileValue;
	}
}

inline
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

inline
TilePosition CenteredTilePosition(u32 absX, u32 absY, u32 absZ) {
	TilePosition pos = {};
	pos.absX = absX;
	pos.absY = absY;
	pos.absZ = absZ;
	return pos;
}

inline
bool IsTileValueEmpty(u32 tileValue) {
	return tileValue == 1 || tileValue == 3 || tileValue == 4;
}

inline
bool IsWorldPointEmpty(TileMap& tilemap, u32 absX, u32 absY, u32 absZ) {
	TileChunkPosition chunkPos = GetTileChunkPosition(tilemap, absX, absY, absZ);
	TileChunk* chunk = GetTileChunk(tilemap, chunkPos.chunkX, chunkPos.chunkY, chunkPos.chunkZ);
	u32 tileValue = GetTileValue(tilemap, chunk, chunkPos.relTileX, chunkPos.relTileY);
	bool isEmpty = IsTileValueEmpty(tileValue);
	return isEmpty;
}

inline
bool IsWorldPointEmpty(TileMap& tilemap, TilePosition& position) {
	return IsWorldPointEmpty(tilemap, position.absX, position.absY, position.absZ);
}

inline
bool AreOnTheSameTile(TilePosition& first, TilePosition& second) {
	bool result = first.absX == second.absX &&
		first.absY == second.absY &&
		first.absZ == second.absZ;
	return result;
}

inline
void FixTilePosition(TileMap& tilemap, TilePosition& position) {
	i32 offsetX = RoundF32ToI32(position.offset.X / tilemap.tileSizeInMeters.X);
	i32 offsetY = RoundF32ToI32(position.offset.Y / tilemap.tileSizeInMeters.Y);
	position.absX += offsetX;
	position.absY += offsetY;
	position.offset.X -= offsetX * tilemap.tileSizeInMeters.X;
	position.offset.Y -= offsetY * tilemap.tileSizeInMeters.Y;
	Assert(position.offset.X >= -tilemap.tileSizeInMeters.X / 2.0f);
	Assert(position.offset.Y >= -tilemap.tileSizeInMeters.Y / 2.0f);
	Assert(position.offset.X <= tilemap.tileSizeInMeters.X / 2.0f);
	Assert(position.offset.Y <= tilemap.tileSizeInMeters.Y / 2.0f);
}

inline
TilePosition OffsetPosition(TileMap& tilemap, TilePosition& position, f32 offsetX, f32 offsetY) {
	TilePosition newPosition = position;
	newPosition.offset += V2{ offsetX, offsetY };
	FixTilePosition(tilemap, newPosition);
	return newPosition;
}

inline
TilePosition OffsetPosition(TileMap& tilemap, TilePosition& position, V2 offset) {
	TilePosition newPosition = position;
	newPosition.offset += offset;
	FixTilePosition(tilemap, newPosition);
	return newPosition;
}

struct DiffTilePosition {
	V2 dXY;
	f32 dZ;
};

inline
DiffTilePosition Subtract(TileMap& tilemap, TilePosition& first, TilePosition& second) {
	DiffTilePosition diff = {};
	V2 firstMeters = { scast(f32, first.absX * tilemap.tileSizeInMeters.X) + first.offset.X,
					   scast(f32, first.absY * tilemap.tileSizeInMeters.Y) + first.offset.Y };
	V2 secondMeters = { scast(f32, second.absX * tilemap.tileSizeInMeters.X) + second.offset.X,
						scast(f32, second.absY * tilemap.tileSizeInMeters.Y) + second.offset.Y };
	diff.dXY = firstMeters - secondMeters;
	diff.dZ = scast(f32, first.absZ) - scast(f32, second.absZ);
	return diff;
}
