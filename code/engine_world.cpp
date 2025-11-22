#include "engine_world.h"

inline
TileChunkPosition GetTileChunkPosition(TileMap& tilemap, i32 absX, i32 absY, i32 absZ) {
	TileChunkPosition chunkPos;
	chunkPos.chunkX = absX >> tilemap.chunkShift;
	chunkPos.chunkY = absY >> tilemap.chunkShift;
	chunkPos.chunkZ = absZ;
	i32 relTileX = absX & tilemap.chunkMask;
	i32 relTileY = absY & tilemap.chunkMask;
	chunkPos.offset = V2{ scast(f32, relTileX) - scast(f32, tilemap.chunkDim) / 2.f,
						  scast(f32, relTileY) - scast(f32, tilemap.chunkDim) / 2.f };
	return chunkPos;
}

inline
TileChunk* GetTileChunk(TileMap& tilemap, i32 chunkX, i32 chunkY, i32 chunkZ, MemoryArena* arena = 0) {
	static_assert((ArrayCount(tilemap.hashTileChunks) & (ArrayCount(tilemap.hashTileChunks) - 1)) == 0 &&
					"hashValue is ANDed with a mask based with assert that the size of hashTileChunks is power of two");
	// TODO: Better hash function
	u32 hashValue = 2767 * chunkX + 4517 * chunkY + 5099 * chunkZ;
	hashValue &= ArrayCount(tilemap.hashTileChunks) - 1;

	TileChunk* chunk = tilemap.hashTileChunks[hashValue];
	if (!chunk && arena) {
		// Add chunk at the beginning of the linked list
		chunk = ptrcast(TileChunk, PushStructSize(*arena, TileChunk));
		chunk->chunkX = chunkX;
		chunk->chunkY = chunkY;
		chunk->chunkZ = chunkZ;
		tilemap.hashTileChunks[hashValue] = chunk;
		return chunk;
	}
	while (chunk) {
		if (chunkX == chunk->chunkX &&
			chunkY == chunk->chunkY &&
			chunkZ == chunk->chunkZ)
		{
			return chunk;
		}
		if (!chunk->next && arena) {
			// Add chunk at the end of the linked list
			chunk->next = ptrcast(TileChunk, PushStructSize(*arena, TileChunk));
			chunk->next->chunkX = chunkX;
			chunk->next->chunkY = chunkY;
			chunk->next->chunkZ = chunkZ;
			return chunk->next;
		}
		chunk = chunk->next;
	}
	return 0;
}

#if 0
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
	TileChunk* chunk = GetTileChunk(tilemap, chunkPos.chunkX, chunkPos.chunkY, chunkPos.chunkZ, &arena);
	Assert(chunk);
	if (!chunk) {
		return;
	}
	if (!chunk->tiles) {
		u32 tileChunkSize = tilemap.chunkDim * tilemap.chunkDim;
		chunk->tiles = ptrcast(u32, PushArray(arena, tileChunkSize, u32));
		for (u32 tileIndex = 0; tileIndex < tileChunkSize; tileIndex++) {
			chunk->tiles[tileIndex] = 1;
		}
	}
	SetTileValue(tilemap, chunk, chunkPos.relTileX, chunkPos.relTileY, tileValue);
}
#endif

inline
TilePosition CenteredTilePosition(i32 absX, i32 absY, i32 absZ) {
	TilePosition pos = {};
	pos.absX = absX;
	pos.absY = absY;
	pos.absZ = absZ;
	return pos;
}

#if 0
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
#endif

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
	// TODO: Think what if absX, absY is 2^32-1 and 2^32 (do we have a bug with overflowing again?)
	diff.dXY = {
		scast(f32, first.absX - second.absX) * tilemap.tileSizeInMeters.X + (first.offset.X - second.offset.X),
		scast(f32, first.absY - second.absY) * tilemap.tileSizeInMeters.Y + (first.offset.Y - second.offset.Y)
	};
	diff.dZ = scast(f32, first.absZ - second.absZ);
	return diff;
}
