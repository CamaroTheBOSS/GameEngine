#include "engine_world.h"

#define CHUNK_DIM_IN_TILES 16
#define CHUNK_SAFE_MARGIN 256

internal
WorldPosition GetChunkPositionFromWorldPosition(World& world, i32 absX, i32 absY, i32 absZ) {
	WorldPosition chunkPos = {};
	// TODO do something with f32 precision, when absX,absY will be high, precision might be lost
	chunkPos.chunkX = FloorF32ToI32(absX / scast(f32, CHUNK_DIM_IN_TILES));
	chunkPos.chunkY = FloorF32ToI32(absY / scast(f32, CHUNK_DIM_IN_TILES));
	chunkPos.chunkZ = absZ;
	i32 relWorldX = absX - chunkPos.chunkX * CHUNK_DIM_IN_TILES;
	i32 relWorldY = absY - chunkPos.chunkY * CHUNK_DIM_IN_TILES;
	chunkPos.offset = V2{ (scast(f32, relWorldX) - CHUNK_DIM_IN_TILES / 2.f) * world.tileSizeInMeters.X,
						  (scast(f32, relWorldY) - CHUNK_DIM_IN_TILES / 2.f) * world.tileSizeInMeters.Y };
	return chunkPos;
}

internal
WorldChunk* GetWorldChunk(World& world, i32 chunkX, i32 chunkY, i32 chunkZ, MemoryArena* arena = 0) {
	static_assert((ArrayCount(world.hashWorldChunks) & (ArrayCount(world.hashWorldChunks) - 1)) == 0 &&
					"hashValue is ANDed with a mask based with assert that the size of hashWorldChunks is power of two");
	Assert(chunkX > INT32_MIN + CHUNK_SAFE_MARGIN);
	Assert(chunkX < INT32_MAX - CHUNK_SAFE_MARGIN);
	Assert(chunkY > INT32_MIN + CHUNK_SAFE_MARGIN);
	Assert(chunkY < INT32_MAX - CHUNK_SAFE_MARGIN);
	Assert(chunkZ > INT32_MIN + CHUNK_SAFE_MARGIN);
	Assert(chunkZ < INT32_MAX - CHUNK_SAFE_MARGIN);

	// TODO: Better hash function
	u32 hashValue = 2767 * chunkX + 4517 * chunkY + 5099 * chunkZ;
	hashValue &= ArrayCount(world.hashWorldChunks) - 1;

	WorldChunk* chunk = world.hashWorldChunks[hashValue];
	if (!chunk && arena) {
		// Add chunk at the beginning of the linked list
		chunk = ptrcast(WorldChunk, PushStructSize(*arena, WorldChunk));
		chunk->chunkX = chunkX;
		chunk->chunkY = chunkY;
		chunk->chunkZ = chunkZ;
		world.hashWorldChunks[hashValue] = chunk;
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
			chunk->next = ptrcast(WorldChunk, PushStructSize(*arena, WorldChunk));
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
u32 GetWorldValue(World& world, WorldChunk* chunk, u32 relX, u32 relY) {
	if (chunk && chunk->tiles && relY < world.chunkDim && relX < world.chunkDim) {
		return chunk->tiles[relY * world.chunkDim + relX];
	}
	return 0;
}

inline
u32 GetWorldValue(World& world, u32 absX, u32 absY, u32 absZ) {
	WorldChunkPosition chunkPos = GetWorldChunkPosition(world, absX, absY, absZ);
	WorldChunk* chunk = GetWorldChunk(world, chunkPos.chunkX, chunkPos.chunkY, chunkPos.chunkZ);
	u32 value = GetWorldValue(world, chunk, chunkPos.relWorldX, chunkPos.relWorldY);
	return value;
}

inline
u32 GetWorldValue(World& world, WorldPosition& position) {
	return GetWorldValue(world, position.absX, position.absY, position.absZ);
}

inline
void SetWorldValue(World& world, WorldChunk* chunk, u32 relX, u32 relY, u32 tileValue) {
	if (chunk && chunk->tiles && relY < world.chunkDim && relX < world.chunkDim) {
		chunk->tiles[relY * world.chunkDim + relX] = tileValue;
	}
}


inline
void SetWorldValue(MemoryArena& arena, World& world, u32 absX, u32 absY, u32 absZ, u32 tileValue) {
	WorldChunkPosition chunkPos = GetWorldChunkPosition(world, absX, absY, absZ);
	WorldChunk* chunk = GetWorldChunk(world, chunkPos.chunkX, chunkPos.chunkY, chunkPos.chunkZ, &arena);
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
	SetWorldValue(world, chunk, chunkPos.relWorldX, chunkPos.relWorldY, tileValue);
}
#endif

internal
WorldPosition CenteredWorldPosition(i32 absX, i32 absY, i32 absZ) {
	WorldPosition pos = {};
	pos.chunkX = absX;
	pos.chunkY = absY;
	pos.chunkZ = absZ;
	return pos;
}

#if 0
inline
bool IsWorldValueEmpty(u32 tileValue) {
	return tileValue == 1 || tileValue == 3 || tileValue == 4;
}

inline
bool IsWorldPointEmpty(World& world, u32 absX, u32 absY, u32 absZ) {
	WorldChunkPosition chunkPos = GetWorldChunkPosition(world, absX, absY, absZ);
	WorldChunk* chunk = GetWorldChunk(world, chunkPos.chunkX, chunkPos.chunkY, chunkPos.chunkZ);
	u32 tileValue = GetWorldValue(world, chunk, chunkPos.relWorldX, chunkPos.relWorldY);
	bool isEmpty = IsWorldValueEmpty(tileValue);
	return isEmpty;
}

inline
bool IsWorldPointEmpty(World& world, WorldPosition& position) {
	return IsWorldPointEmpty(world, position.absX, position.absY, position.absZ);
}

inline
bool AreOnTheSameWorld(WorldPosition& first, WorldPosition& second) {
	bool result = first.absX == second.absX &&
		first.absY == second.absY &&
		first.absZ == second.absZ;
	return result;
}
#endif

internal
void FixWorldPosition(World& world, WorldPosition& position) {
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
WorldPosition OffsetPosition(World& world, WorldPosition& position, f32 offsetX, f32 offsetY) {
	WorldPosition newPosition = position;
	newPosition.offset += V2{ offsetX, offsetY };
	FixWorldPosition(world, newPosition);
	return newPosition;
}

internal
WorldPosition OffsetPosition(World& world, WorldPosition& position, V2 offset) {
	WorldPosition newPosition = position;
	newPosition.offset += offset;
	FixWorldPosition(world, newPosition);
	return newPosition;
}

struct DiffWorldPosition {
	V2 dXY;
	f32 dZ;
};

internal
DiffWorldPosition Subtract(World& world, WorldPosition& first, WorldPosition& second) {
	DiffWorldPosition diff = {};
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
