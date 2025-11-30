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

struct DiffWorldPosition {
	V2 dXY;
	f32 dZ;
};

// TODO change name to EntityStorageBlock
struct LowEntityBlock {
	u32 entityCount;
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

enum EntityType {
	EntityType_Nonexist = 0,
	EntityType_Player,
	EntityType_Wall,
	EntityType_Familiar,
	EntityType_Monster,
	EntityType_Sword,
};

enum EntityFlag : u32 {
	EntityFlag_StopsOnCollide	= (1 << 0),
	EntityFlag_NonSpatial		= (1 << 1),
};

struct HP {
	u32 amount;
	u32 max;
};

struct HitPoints {
	u32 count;
	HP hitPoints[16];
};

struct Entity {
	u32 storageIndex;
	V2 pos;
	V2 vel;
	EntityType type;
	WorldPosition worldPos;
	V2 size;
	u32 faceDir;
	u32 highEntityIndex;
	HitPoints hitPoints;
	u32 flags;
	Entity* sword;
	f32 distanceRemaining;
};

struct EntityStorage {
	Entity entity;
};

struct World {
	u32 tileCountX;
	u32 tileCountY;
	V2 tileSizeInMeters;
	V2 chunkSizeInMeters;

	MemoryArena arena;
	WorldChunk* hashWorldChunks[4096];
	LowEntityBlock* freeEntityBlockList;

	u32 storageEntityCount;
	EntityStorage storageEntities[10000];
};

// Function declaration to help Intellisense got some sense :)
internal WorldChunk* GetWorldChunk(World& world, i32 chunkX, i32 chunkY, i32 chunkZ, MemoryArena* arena = 0);
internal WorldPosition OffsetWorldPosition(World& world, WorldPosition& position, V2 offset);
internal WorldPosition OffsetWorldPosition(World& world, WorldPosition& position, f32 offsetX, f32 offsetY);
internal DiffWorldPosition Subtract(World& world, WorldPosition& first, WorldPosition& second);
internal void ChangeEntityChunkLocation(World& world, MemoryArena& arena, u32 lowEntityIndex, Entity& entity, WorldPosition* oldPos, WorldPosition& newPos);
internal WorldPosition GetChunkPositionFromWorldPosition(World& world, i32 absX, i32 absY, i32 absZ);
inline void SetFlag(Entity& entity, u32 flag);
inline void ClearFlag(Entity& entity, u32 flag);
inline bool IsFlagSet(Entity& entity, u32 flag);
inline WorldPosition NullPosition();