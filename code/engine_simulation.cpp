#include "engine_simulation.h"

internal
u32 AddEntity(World& world, EntityStorage& storage) {
	Assert(world.storageEntityCount < ArrayCount(world.storageEntities));
	if (world.storageEntityCount >= ArrayCount(world.storageEntities)) {
		return 0;
	}
	ChangeEntityChunkLocation(world, world.arena, world.storageEntityCount, 0, storage.entity.worldPos);
	world.storageEntities[world.storageEntityCount++] = storage;
	return world.storageEntityCount - 1;
}

internal
EntityStorage* GetEntityStorage(World& world, u32 storageEntityIndex) {
	EntityStorage* storage = 0;
	// TODO: should I keep this assertion?
	//Assert(storageEntityIndex > 0 && storageEntityIndex < state->storageEntityCount);
	if (storageEntityIndex > 0 && storageEntityIndex < world.storageEntityCount) {
		storage = &world.storageEntities[storageEntityIndex];
	}
	return storage;
}

internal
EntityHash* GetHashFromStorageIndex(SimRegion& simRegion, u32 storageEntityIndex) {
	static_assert((ArrayCount(simRegion.entityHash) & (ArrayCount(simRegion.entityHash) - 1)) == 0 &&
		"hashValue is ANDed with a mask based with assert that the size of hashWorldChunks is power of two");
	for (u32 offset = 0; offset < ArrayCount(simRegion.entityHash); offset++) {
		u32 hashMask = (ArrayCount(simRegion.entityHash) - 1);
		u32 hashIndex = (storageEntityIndex + offset) & hashMask;
		EntityHash* entityHash = simRegion.entityHash + hashIndex;
		if (entityHash->index == 0 || entityHash->index == storageEntityIndex) {
			return entityHash;
		}
	}
	Assert(!"We are out of space in hash table!");
	return 0;
}

inline
Entity* GetEntityByStorageIndex(SimRegion& simRegion, u32 storageEntityIndex) {
	EntityHash* entityHash = GetHashFromStorageIndex(simRegion, storageEntityIndex);
	if (entityHash) {
		return entityHash->ptr;
	}
	return 0;
}

inline
void AddEntityToSim(SimRegion& simRegion, Entity& entity) {
	Assert(simRegion.entityCount < simRegion.maxEntityCount);
	if (simRegion.entityCount >= simRegion.maxEntityCount) {
		return;
	}
	EntityHash* entityHash = GetHashFromStorageIndex(simRegion, entity.storageIndex);
	Assert(entity.storageIndex != 0);
	Assert(!entityHash->ptr && "entity hash entry in hash table must not exist");
	simRegion.entities[simRegion.entityCount] = entity;
	entityHash->index = entity.storageIndex;
	entityHash->ptr = &simRegion.entities[simRegion.entityCount];
	simRegion.entityCount++;
	return;
}

internal
SimRegion* BeginSimulation(MemoryArena& simArena, World& world,
	WorldPosition& origin, Rect2 bounds)
{
	SimRegion* simRegion = ptrcast(SimRegion, PushStructSize(simArena, SimRegion));
	simRegion->entityCount = 0;
	simRegion->maxEntityCount = ArrayCount(simRegion->entities);
	simRegion->origin = origin;
	simRegion->bounds = bounds;
	WorldPosition minChunk = OffsetWorldPosition(world, origin, GetMinCorner(bounds));
	WorldPosition maxChunk = OffsetWorldPosition(world, origin, GetMaxCorner(bounds));
	for (i32 chunkY = minChunk.chunkY; chunkY <= maxChunk.chunkY; chunkY++) {
		for (i32 chunkX = minChunk.chunkX; chunkX <= maxChunk.chunkX; chunkX++) {
			WorldChunk* chunk = GetWorldChunk(world, chunkX, chunkY, origin.chunkZ);
			if (!chunk) {
				continue;
			}
			for (LowEntityBlock* entities = chunk->entities; entities; entities = entities->next) {
				for (u32 index = 0; index < entities->entityCount; index++) {
					u32 storageEntityIndex = entities->entityIndexes[index];
					EntityStorage* storage = GetEntityStorage(world, storageEntityIndex);
					Assert(storage);
					if (!storage) {
						continue;
					}
					DiffWorldPosition diff = Subtract(world, storage->entity.worldPos, simRegion->origin);
					if (IsInRectangle(simRegion->bounds, diff.dXY)) {
						Entity entity = storage->entity;
						entity.storageIndex = storageEntityIndex;
						entity.pos = diff.dXY;
						AddEntityToSim(*simRegion, entity);
					}
				}
			}
		}
	}
	return simRegion;
}

internal
void EndSimulation(MemoryArena& simArena, SimRegion& simRegion, World& world) {
	for (u32 entityIndex = 0; entityIndex < simRegion.entityCount; entityIndex++) {
		Entity* entity = simRegion.entities + entityIndex;
		EntityStorage* storage = GetEntityStorage(world, entity->storageIndex);
		Assert(storage && entity);
		if (storage && entity) {
			storage->entity = *entity;
		}
	}
	ZeroMemory(simArena);
}