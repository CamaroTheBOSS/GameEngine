#pragma once
#include "engine_common.h"
#include "engine_world.h"

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
