#include "engine_simulation.h"

internal
u32 AddEntity(World& world, EntityStorage& storage) {
	Assert(world.storageEntityCount < ArrayCount(world.storageEntities));
	if (world.storageEntityCount >= ArrayCount(world.storageEntities)) {
		return 0;
	}
	ChangeEntityChunkLocation(world, world.arena, world.storageEntityCount, 
		storage.entity, 0, storage.entity.worldPos
	);
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
void TryAddEntityToSim(SimRegion& simRegion, World& world, u32 storageEntityIndex, Entity& entity) {
	V3 diff = Subtract(world, entity.worldPos, simRegion.origin);
	if (!IsInRectangle(simRegion.bounds, diff)) {
		return;
	}
	Assert(simRegion.entityCount < simRegion.maxEntityCount);
	Assert(storageEntityIndex != 0);
	if (simRegion.entityCount >= simRegion.maxEntityCount) {
		return;
	}
	EntityHash* entityHash = GetHashFromStorageIndex(simRegion, storageEntityIndex);
	Assert(!entityHash->ptr && "entity hash entry in hash table must not exist");

	Entity* simEntity = &simRegion.entities[simRegion.entityCount];
	*simEntity = entity;
	simEntity->storageIndex = storageEntityIndex;
	simEntity->pos = diff;

	entityHash->index = storageEntityIndex;
	entityHash->ptr = simEntity;
	simRegion.entityCount++;
	return;
}

internal
SimRegion* BeginSimulation(MemoryArena& simArena, World& world,
	WorldPosition& origin, Rect3 bounds)
{
	SimRegion* simRegion = ptrcast(SimRegion, PushStructSize(simArena, SimRegion));
	simRegion->entityCount = 0;
	simRegion->maxEntityCount = ArrayCount(simRegion->entities);
	simRegion->origin = origin;
	simRegion->bounds = bounds;
	WorldPosition minChunk = OffsetWorldPosition(world, origin, GetMinCorner(bounds));
	WorldPosition maxChunk = OffsetWorldPosition(world, origin, GetMaxCorner(bounds));
	for (i32 chunkZ = minChunk.chunkZ; chunkZ <= maxChunk.chunkZ; chunkZ++) {
		for (i32 chunkY = minChunk.chunkY; chunkY <= maxChunk.chunkY; chunkY++) {
			for (i32 chunkX = minChunk.chunkX; chunkX <= maxChunk.chunkX; chunkX++) {
				WorldChunk* chunk = GetWorldChunk(world, chunkX, chunkY, chunkZ);
				if (!chunk) {
					continue;
				}
				for (LowEntityBlock* entities = chunk->entities; entities; entities = entities->next) {
					for (u32 index = 0; index < entities->entityCount; index++) {
						u32 storageEntityIndex = entities->entityIndexes[index];
						EntityStorage* storage = GetEntityStorage(world, storageEntityIndex);
						TryAddEntityToSim(*simRegion, world, storageEntityIndex, storage->entity);
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