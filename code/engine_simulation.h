#pragma once
#include "engine_common.h"
#include "engine_world.h"

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

struct EntityHash {
	Entity* ptr;
	u32 index;
};

struct SimRegion {
	WorldPosition origin;
	Rect2 bounds;

	u32 maxEntityCount;
	u32 entityCount;
	Entity entities[1024];

	EntityHash entityHash[4096];
};
