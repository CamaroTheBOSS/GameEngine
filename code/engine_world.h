#pragma once
#include "engine_common.h"

struct WorldPosition {
	// 28 bytes = chunk pos, 4 bytes = tile pos inside chunk
	i32 chunkX;
	i32 chunkY;
	i32 chunkZ;

	// Pos inside world chunk in meters
	V3 offset;
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
	V3 pos;
	V3 vel;
	EntityType type;
	WorldPosition worldPos;
	V2 size; // TODO: should be V3?
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

struct PairwiseCollision {
	u32 storageIndex1;
	u32 storageIndex2;
	PairwiseCollision* next;
};

struct World {
	u32 tileCountX;
	u32 tileCountY;
	V3 tileSizeInMeters;
	V3 chunkSizeInMeters;

	MemoryArena arena;
	WorldChunk* hashWorldChunks[4096];
	LowEntityBlock* freeEntityBlockList;

	u32 storageEntityCount;
	EntityStorage storageEntities[10000];

	PairwiseCollision* hashCollisions[4096];
	PairwiseCollision* freeCollisionsList;
};

// Function declaration to help Intellisense got some sense :)
internal WorldChunk* GetWorldChunk(World& world, i32 chunkX, i32 chunkY, i32 chunkZ, MemoryArena* arena = 0);
internal WorldPosition OffsetWorldPosition(World& world, WorldPosition& position, V3 offset);
internal WorldPosition OffsetWorldPosition(World& world, WorldPosition& position, f32 offsetX, f32 offsetY, f32 offsetZ);
internal V3 Subtract(World& world, WorldPosition& first, WorldPosition& second);
internal void ChangeEntityChunkLocation(World& world, MemoryArena& arena, u32 lowEntityIndex, Entity& entity, WorldPosition* oldPos, WorldPosition& newPos);
internal WorldPosition GetChunkPositionFromWorldPosition(World& world, i32 absX, i32 absY, i32 absZ);
inline void SetFlag(Entity& entity, u32 flag);
inline void ClearFlag(Entity& entity, u32 flag);
inline bool IsFlagSet(Entity& entity, u32 flag);
inline WorldPosition NullPosition();