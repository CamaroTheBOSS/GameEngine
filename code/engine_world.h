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
	EntityType_Stairs,
	EntityType_Space,
};

enum EntityFlag : u32 {
	EntityFlag_StopsOnCollide	= (1 << 0),
	EntityFlag_NonSpatial		= (1 << 1),
	EntityFlag_Overlaps			= (1 << 2),
	EntityFlag_Movable			= (1 << 3),
	EntityFlag_Traversable		= (1 << 4),
};

struct HP {
	u32 amount;
	u32 max;
};

struct HitPoints {
	u32 count;
	HP hitPoints[16];
};

struct CollisionVolume {
	V3 offsetPos;
	V3 size;
};

struct CollisionVolumeGroup {
	CollisionVolume totalVolume; // Bounding box volume
	u32 volumeCount;
	CollisionVolume* volumes;
};

struct Entity {
	u32 storageIndex;
	V3 pos;
	V3 vel;
	EntityType type;
	WorldPosition worldPos;
	CollisionVolumeGroup* collision;
	f32 faceDir;
	u32 highEntityIndex;
	HitPoints hitPoints;
	u32 flags;
	// Only Hero
	Entity* sword;
	// Only Sword (or throwable)
	f32 distanceRemaining;
	f32 timeRemaining;
	// Only stairs
	V3 walkableDim;
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
internal WorldChunk* GetWorldChunk(World& world, WorldPosition& pos, MemoryArena* arena = 0);
internal f32 GetHeightFromTheClosestGroundLevel(World& world, WorldPosition& position);
internal f32 GetDistanceToTheClosestGroundLevel(World& world, WorldPosition& pos);
internal WorldPosition OffsetWorldPosition(World& world, WorldPosition& position, V3 offset);
internal WorldPosition OffsetWorldPosition(World& world, WorldPosition& position, f32 offsetX, f32 offsetY, f32 offsetZ);
internal V3 Subtract(World& world, WorldPosition& first, WorldPosition& second);
internal void ChangeEntityChunkLocation(World& world, MemoryArena& arena, u32 lowEntityIndex, Entity& entity, WorldPosition* oldPos, WorldPosition& newPos);
internal WorldPosition GetChunkPositionFromWorldPosition(World& world, i32 absX, i32 absY, i32 absZ, V3 offset = V3{0.f, 0.f, 0.f});
inline void SetFlag(Entity& entity, u32 flag);
inline void ClearFlag(Entity& entity, u32 flag);
inline bool IsFlagSet(Entity& entity, u32 flag);
inline WorldPosition NullPosition();