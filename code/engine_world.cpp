#include "engine_world.h"

#define CHUNK_DIM_IN_TILES 16
#define CHUNK_SAFE_MARGIN 256

internal
TilePosition GetChunkPositionFromTilePosition(World& world, i32 absX, i32 absY, i32 absZ) {
	TilePosition chunkPos = {};
	// TODO do something with f32 precision, when absX,absY will be high, precision might be lost
	chunkPos.chunkX = FloorF32ToI32(absX / scast(f32, CHUNK_DIM_IN_TILES));
	chunkPos.chunkY = FloorF32ToI32(absY / scast(f32, CHUNK_DIM_IN_TILES));
	chunkPos.chunkZ = absZ;
	i32 relTileX = absX - chunkPos.chunkX * CHUNK_DIM_IN_TILES;
	i32 relTileY = absY - chunkPos.chunkY * CHUNK_DIM_IN_TILES;
	chunkPos.offset = V2{ (scast(f32, relTileX) - CHUNK_DIM_IN_TILES / 2.f) * world.tileSizeInMeters.X,
						  (scast(f32, relTileY) - CHUNK_DIM_IN_TILES / 2.f) * world.tileSizeInMeters.Y };
	return chunkPos;
}

internal
TileChunk* GetTileChunk(World& world, i32 chunkX, i32 chunkY, i32 chunkZ, MemoryArena* arena = 0) {
	static_assert((ArrayCount(world.hashTileChunks) & (ArrayCount(world.hashTileChunks) - 1)) == 0 &&
					"hashValue is ANDed with a mask based with assert that the size of hashTileChunks is power of two");
	Assert(chunkX > INT32_MIN + CHUNK_SAFE_MARGIN);
	Assert(chunkX < INT32_MAX - CHUNK_SAFE_MARGIN);
	Assert(chunkY > INT32_MIN + CHUNK_SAFE_MARGIN);
	Assert(chunkY < INT32_MAX - CHUNK_SAFE_MARGIN);
	Assert(chunkZ > INT32_MIN + CHUNK_SAFE_MARGIN);
	Assert(chunkZ < INT32_MAX - CHUNK_SAFE_MARGIN);

	// TODO: Better hash function
	u32 hashValue = 2767 * chunkX + 4517 * chunkY + 5099 * chunkZ;
	hashValue &= ArrayCount(world.hashTileChunks) - 1;

	TileChunk* chunk = world.hashTileChunks[hashValue];
	if (!chunk && arena) {
		// Add chunk at the beginning of the linked list
		chunk = ptrcast(TileChunk, PushStructSize(*arena, TileChunk));
		chunk->chunkX = chunkX;
		chunk->chunkY = chunkY;
		chunk->chunkZ = chunkZ;
		world.hashTileChunks[hashValue] = chunk;
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
u32 GetTileValue(World& world, TileChunk* chunk, u32 relX, u32 relY) {
	if (chunk && chunk->tiles && relY < world.chunkDim && relX < world.chunkDim) {
		return chunk->tiles[relY * world.chunkDim + relX];
	}
	return 0;
}

inline
u32 GetTileValue(World& world, u32 absX, u32 absY, u32 absZ) {
	TileChunkPosition chunkPos = GetTileChunkPosition(world, absX, absY, absZ);
	TileChunk* chunk = GetTileChunk(world, chunkPos.chunkX, chunkPos.chunkY, chunkPos.chunkZ);
	u32 value = GetTileValue(world, chunk, chunkPos.relTileX, chunkPos.relTileY);
	return value;
}

inline
u32 GetTileValue(World& world, TilePosition& position) {
	return GetTileValue(world, position.absX, position.absY, position.absZ);
}

inline
void SetTileValue(World& world, TileChunk* chunk, u32 relX, u32 relY, u32 tileValue) {
	if (chunk && chunk->tiles && relY < world.chunkDim && relX < world.chunkDim) {
		chunk->tiles[relY * world.chunkDim + relX] = tileValue;
	}
}


inline
void SetTileValue(MemoryArena& arena, World& world, u32 absX, u32 absY, u32 absZ, u32 tileValue) {
	TileChunkPosition chunkPos = GetTileChunkPosition(world, absX, absY, absZ);
	TileChunk* chunk = GetTileChunk(world, chunkPos.chunkX, chunkPos.chunkY, chunkPos.chunkZ, &arena);
	Assert(chunk);
	if (!chunk) {
		return;
	}
	if (!chunk->tiles) {
		u32 tileChunkSize = world.chunkDim * world.chunkDim;
		chunk->tiles = ptrcast(u32, PushArray(arena, tileChunkSize, u32));
		for (u32 tileIndex = 0; tileIndex < tileChunkSize; tileIndex++) {
			chunk->tiles[tileIndex] = 1;
		}
	}
	SetTileValue(world, chunk, chunkPos.relTileX, chunkPos.relTileY, tileValue);
}
#endif

internal
TilePosition CenteredTilePosition(i32 absX, i32 absY, i32 absZ) {
	TilePosition pos = {};
	pos.chunkX = absX;
	pos.chunkY = absY;
	pos.chunkZ = absZ;
	return pos;
}

#if 0
inline
bool IsTileValueEmpty(u32 tileValue) {
	return tileValue == 1 || tileValue == 3 || tileValue == 4;
}

inline
bool IsWorldPointEmpty(World& world, u32 absX, u32 absY, u32 absZ) {
	TileChunkPosition chunkPos = GetTileChunkPosition(world, absX, absY, absZ);
	TileChunk* chunk = GetTileChunk(world, chunkPos.chunkX, chunkPos.chunkY, chunkPos.chunkZ);
	u32 tileValue = GetTileValue(world, chunk, chunkPos.relTileX, chunkPos.relTileY);
	bool isEmpty = IsTileValueEmpty(tileValue);
	return isEmpty;
}

inline
bool IsWorldPointEmpty(World& world, TilePosition& position) {
	return IsWorldPointEmpty(world, position.absX, position.absY, position.absZ);
}

inline
bool AreOnTheSameTile(TilePosition& first, TilePosition& second) {
	bool result = first.absX == second.absX &&
		first.absY == second.absY &&
		first.absZ == second.absZ;
	return result;
}
#endif

internal
void FixTilePosition(World& world, TilePosition& position) {
	i32 offsetX = RoundF32ToI32(position.offset.X / world.chunkSizeInMeters.X);
	i32 offsetY = RoundF32ToI32(position.offset.Y / world.chunkSizeInMeters.Y);
	position.chunkX += offsetX;
	position.chunkY += offsetY;
	position.offset.X -= offsetX * world.chunkSizeInMeters.X;
	position.offset.Y -= offsetY * world.chunkSizeInMeters.Y;
	Assert(position.offset.X >= -world.chunkSizeInMeters.X / 2.0f);
	Assert(position.offset.Y >= -world.chunkSizeInMeters.Y / 2.0f);
	Assert(position.offset.X <= world.chunkSizeInMeters.X / 2.0f);
	Assert(position.offset.Y <= world.chunkSizeInMeters.Y / 2.0f);
}

internal
TilePosition OffsetPosition(World& world, TilePosition& position, f32 offsetX, f32 offsetY) {
	TilePosition newPosition = position;
	newPosition.offset += V2{ offsetX, offsetY };
	FixTilePosition(world, newPosition);
	return newPosition;
}

internal
TilePosition OffsetPosition(World& world, TilePosition& position, V2 offset) {
	TilePosition newPosition = position;
	newPosition.offset += offset;
	FixTilePosition(world, newPosition);
	return newPosition;
}

struct DiffTilePosition {
	V2 dXY;
	f32 dZ;
};

internal
DiffTilePosition Subtract(World& world, TilePosition& first, TilePosition& second) {
	DiffTilePosition diff = {};
	// TODO: Think what if absX, absY is 2^32-1 and 2^32 (do we have a bug with overflowing again?)
	diff.dXY = {
		scast(f32, first.chunkX - second.chunkX) * world.chunkSizeInMeters.X + (first.offset.X - second.offset.X),
		scast(f32, first.chunkY - second.chunkY) * world.chunkSizeInMeters.Y + (first.offset.Y - second.offset.Y)
	};
	diff.dZ = scast(f32, first.chunkZ - second.chunkZ);
	return diff;
}

internal
void InitializeWorld(World& world) {
	world.tileCountX = 17;
	world.tileCountY = 9;
	world.tileSizeInMeters = V2{ 1.4f , 1.4f };
	world.chunkSizeInMeters = CHUNK_DIM_IN_TILES * world.tileSizeInMeters;
}
