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
	EntityType_Wall
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
	bool collide;
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

internal WorldChunk* GetWorldChunk(World& world, i32 chunkX, i32 chunkY, i32 chunkZ, MemoryArena* arena = 0);
internal WorldPosition OffsetWorldPosition(World& world, WorldPosition& position, V2 offset);
internal WorldPosition OffsetWorldPosition(World& world, WorldPosition& position, f32 offsetX, f32 offsetY);
internal DiffWorldPosition Subtract(World& world, WorldPosition& first, WorldPosition& second);
internal void ChangeEntityChunkLocation(World& world, MemoryArena& arena, u32 lowEntityIndex, WorldPosition* oldPos, WorldPosition& newPos);