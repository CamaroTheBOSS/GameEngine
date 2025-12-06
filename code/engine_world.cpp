#include "engine_world.h"

#define CHUNK_DIM_IN_TILES 16
#define CHUNK_HEIGHT_IN_TILES 1
#define CHUNK_SAFE_MARGIN 256
inline
WorldPosition NullPosition() {
	WorldPosition pos = {};
	pos.chunkX = INT32_MAX;
	pos.chunkY = INT32_MAX;
	pos.chunkZ = INT32_MAX;
	return pos;
}

inline
bool operator==(const WorldPosition& first, const WorldPosition& other) {
	return first.chunkX == other.chunkX &&
		   first.chunkY == other.chunkY &&
		   first.chunkZ == other.chunkZ &&
		   first.offset == other.offset;
}

inline
bool operator!=(const WorldPosition& first, const WorldPosition& other) {
	return !(first == other);
}

inline
void SetFlag(Entity& entity, u32 flag) {
	entity.flags = entity.flags | flag;
}

inline
void ClearFlag(Entity& entity, u32 flag) {
	entity.flags = entity.flags & (~flag);
}

inline
bool IsFlagSet(Entity& entity, u32 flag) {
	bool result = entity.flags & flag;
	return result;
}

internal
WorldPosition GetChunkPositionFromWorldPosition(World& world, i32 absX, i32 absY, i32 absZ, V3 offset) {
	WorldPosition chunkPos = {};
	// TODO(IMPORTANT) do something with f32 precision, when absX,absY,absZ will be high, precision might be lost
	chunkPos.chunkX = FloorF32ToI32(absX / scast(f32, CHUNK_DIM_IN_TILES));
	chunkPos.chunkY = FloorF32ToI32(absY / scast(f32, CHUNK_DIM_IN_TILES));
	chunkPos.chunkZ = FloorF32ToI32(absZ / scast(f32, CHUNK_HEIGHT_IN_TILES));
	i32 relWorldX = absX - chunkPos.chunkX * CHUNK_DIM_IN_TILES;
	i32 relWorldY = absY - chunkPos.chunkY * CHUNK_DIM_IN_TILES;
	i32 relWorldZ = absZ - chunkPos.chunkZ * CHUNK_HEIGHT_IN_TILES;
	// NOTE: offset from center X,Y, but bottom Z part of a chunk
	chunkPos.offset = V3{ (scast(f32, relWorldX) - CHUNK_DIM_IN_TILES / 2.f) * world.tileSizeInMeters.X,
						  (scast(f32, relWorldY) - CHUNK_DIM_IN_TILES / 2.f) * world.tileSizeInMeters.Y,
						   scast(f32, relWorldZ) * world.tileSizeInMeters.Z };
	chunkPos = OffsetWorldPosition(world, chunkPos, offset);
	return chunkPos;
}

internal
WorldChunk* GetWorldChunk(World& world, i32 chunkX, i32 chunkY, i32 chunkZ, MemoryArena* arena) {
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

internal // TODO: delete this function, it is ackward
WorldPosition CenteredWorldPosition(i32 absX, i32 absY, i32 absZ) {
	WorldPosition pos = {};
	pos.chunkX = absX;
	pos.chunkY = absY;
	pos.chunkZ = absZ;
	return pos;
}

internal
bool AreOnTheSameChunk(WorldPosition& first, WorldPosition& second) {
	bool result = first.chunkX == second.chunkX &&
		first.chunkY == second.chunkY &&
		first.chunkZ == second.chunkZ;
	return result;
}


internal
void FixWorldPosition(World& world, WorldPosition& position) {
	i32 offsetX = RoundF32ToI32(position.offset.X / world.chunkSizeInMeters.X);
	i32 offsetY = RoundF32ToI32(position.offset.Y / world.chunkSizeInMeters.Y);
	i32 offsetZ = FloorF32ToI32(position.offset.Z / world.chunkSizeInMeters.Z);
	position.chunkX += offsetX;
	position.chunkY += offsetY;
	position.chunkZ += offsetZ;
	position.offset.X -= offsetX * world.chunkSizeInMeters.X;
	position.offset.Y -= offsetY * world.chunkSizeInMeters.Y;
	position.offset.Z -= offsetZ * world.chunkSizeInMeters.Z;
	Assert(position.offset.X >= -world.chunkSizeInMeters.X / 2.0f);
	Assert(position.offset.X <= world.chunkSizeInMeters.X / 2.0f);
	Assert(position.offset.Y >= -world.chunkSizeInMeters.Y / 2.0f);
	Assert(position.offset.Y <= world.chunkSizeInMeters.Y / 2.0f);
	Assert(position.offset.Z >= 0.f);
	Assert(position.offset.Z <= world.chunkSizeInMeters.Z);

}

internal
WorldPosition MapCameraSpacePositionToWorldPosition(World& world, V3 cameraSpacePosition) {
	WorldPosition position = {};
	position.offset = cameraSpacePosition;
	FixWorldPosition(world, position);
	return position;
}

internal
f32 GetHeightFromTheClosestGroundLevel(World& world, WorldPosition& pos) {
	u32 heightRelToChunk = FloorF32ToU32(pos.offset.Z / world.tileSizeInMeters.Z);
	return pos.offset.Z - heightRelToChunk * world.tileSizeInMeters.Z;
}

internal
f32 GetDistanceToTheClosestGroundLevel(World& world, WorldPosition& pos) {
	u32 heightRelToChunk = RoundF32ToU32(pos.offset.Z / world.tileSizeInMeters.Z);
	return pos.offset.Z - heightRelToChunk * world.tileSizeInMeters.Z;
}

internal
WorldPosition OffsetWorldPosition(World& world, WorldPosition& position, V3 offset) {
	WorldPosition newPosition = position;
	newPosition.offset += offset;
	FixWorldPosition(world, newPosition);
	return newPosition;
}

internal
WorldPosition OffsetWorldPosition(World& world, WorldPosition& position, f32 offsetX, f32 offsetY, f32 offsetZ) {
	return OffsetWorldPosition(world, position, V3{ offsetX, offsetY, offsetZ });
}

internal
V3 Subtract(World& world, WorldPosition& first, WorldPosition& second) {
	// TODO: Think what if absX, absY is 2^32-1 and 2^32 (do we have a bug with overflowing again?)
	V3 diff = {
		scast(f32, first.chunkX - second.chunkX) * world.chunkSizeInMeters.X + (first.offset.X - second.offset.X),
		scast(f32, first.chunkY - second.chunkY) * world.chunkSizeInMeters.Y + (first.offset.Y - second.offset.Y),
		scast(f32, first.chunkZ - second.chunkZ)* world.chunkSizeInMeters.Z + (first.offset.Z - second.offset.Z)
	};
	return diff;
}

internal
void ChangeEntityChunkLocationRaw(World& world, MemoryArena& arena, u32 lowEntityIndex,
	WorldPosition* oldPos, WorldPosition& newPos)
{
	if (oldPos && AreOnTheSameChunk(*oldPos, newPos)) {
		return;
	}
	if (oldPos) {
		WorldChunk* chunk = GetWorldChunk(world, oldPos->chunkX, oldPos->chunkY, oldPos->chunkZ);
		Assert(chunk);
		if (chunk) {
			LowEntityBlock* firstBlock = chunk->entities;
			for (LowEntityBlock* block = firstBlock; block; block = block->next) {
				for (u32 index = 0; index < block->entityCount; index++) {
					if (block->entityIndexes[index] != lowEntityIndex) {
						continue;
					}
					block->entityIndexes[index] = firstBlock->entityIndexes[--firstBlock->entityCount];
					if (firstBlock->entityCount == 0) {
						chunk->entities = firstBlock->next;
						LowEntityBlock* lastFreeBlock = world.freeEntityBlockList;
						world.freeEntityBlockList = firstBlock;
						firstBlock->next = lastFreeBlock;
					}
					block = 0;
					break;
				}
				if (!block) {
					break;
				}
			}
		}
	}
	if (newPos != NullPosition()) {
		WorldChunk* chunk = GetWorldChunk(world, newPos.chunkX, newPos.chunkY, newPos.chunkZ, &arena);
		LowEntityBlock* firstBlock = chunk->entities;
		if (!firstBlock) {
			chunk->entities = ptrcast(LowEntityBlock, PushStructSize(arena, LowEntityBlock));
			firstBlock = chunk->entities;
		}
		if (firstBlock->entityCount == ArrayCount(firstBlock->entityIndexes)) {
			if (world.freeEntityBlockList) {
				Assert(world.freeEntityBlockList->entityCount == 0);
				LowEntityBlock* block = chunk->entities;
				chunk->entities = world.freeEntityBlockList;
				world.freeEntityBlockList = world.freeEntityBlockList->next;
				chunk->entities->next = block;
				firstBlock = chunk->entities;
			}
			else {
				LowEntityBlock* block = chunk->entities;
				chunk->entities = ptrcast(LowEntityBlock, PushStructSize(arena, LowEntityBlock));
				chunk->entities->next = block;
				firstBlock = chunk->entities;
			}
		}
		firstBlock->entityIndexes[firstBlock->entityCount++] = lowEntityIndex;
	}
}

void ChangeEntityChunkLocation(World& world, MemoryArena& arena, u32 lowEntityIndex, Entity& entity,
	WorldPosition* oldPos, WorldPosition& newPos)
{
	ChangeEntityChunkLocationRaw(world, arena, lowEntityIndex, oldPos, newPos);
	if (newPos == NullPosition()) {
		SetFlag(entity, EntityFlag_NonSpatial);
	}
	else {
		ClearFlag(entity, EntityFlag_NonSpatial);
	}
	entity.worldPos = newPos;
}

internal
void InitializeWorld(World& world) {
	world.tileCountX = 17;
	world.tileCountY = 9;
	world.tileSizeInMeters = V3{ 1.4f , 1.4f, 1.4f };
	world.chunkSizeInMeters = V3{
		CHUNK_DIM_IN_TILES * world.tileSizeInMeters.Z,
		CHUNK_DIM_IN_TILES * world.tileSizeInMeters.Y,
		CHUNK_HEIGHT_IN_TILES * world.tileSizeInMeters.Z
	};
	world.storageEntityCount = 1;
}
