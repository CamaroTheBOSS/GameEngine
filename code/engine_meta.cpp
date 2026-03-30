#pragma once
#include "engine_meta.h"

MemberDefinition MembersOf_Entity[] = {
	{ MetaType_V3, "walkableDim", u32(reinterpret_cast<uptr>(&(((Entity*)0)->walkableDim))), 0 },
	{ MetaType_f32, "timeRemaining", u32(reinterpret_cast<uptr>(&(((Entity*)0)->timeRemaining))), 0 },
	{ MetaType_f32, "distanceRemaining", u32(reinterpret_cast<uptr>(&(((Entity*)0)->distanceRemaining))), 0 },
	{ MetaType_Entity, "sword", u32(reinterpret_cast<uptr>(&(((Entity*)0)->sword))), 1 },
	{ MetaType_u32, "flags", u32(reinterpret_cast<uptr>(&(((Entity*)0)->flags))), 0 },
	{ MetaType_HitPoints, "hitPoints", u32(reinterpret_cast<uptr>(&(((Entity*)0)->hitPoints))), 0 },
	{ MetaType_u32, "highEntityIndex", u32(reinterpret_cast<uptr>(&(((Entity*)0)->highEntityIndex))), 0 },
	{ MetaType_f32, "faceDir", u32(reinterpret_cast<uptr>(&(((Entity*)0)->faceDir))), 0 },
	{ MetaType_CollisionVolumeGroup, "collision", u32(reinterpret_cast<uptr>(&(((Entity*)0)->collision))), 1 },
	{ MetaType_WorldPosition, "worldPos", u32(reinterpret_cast<uptr>(&(((Entity*)0)->worldPos))), 0 },
	{ MetaType_EntityType, "type", u32(reinterpret_cast<uptr>(&(((Entity*)0)->type))), 0 },
	{ MetaType_V3, "vel", u32(reinterpret_cast<uptr>(&(((Entity*)0)->vel))), 0 },
	{ MetaType_V3, "pos", u32(reinterpret_cast<uptr>(&(((Entity*)0)->pos))), 0 },
	{ MetaType_u32, "storageIndex", u32(reinterpret_cast<uptr>(&(((Entity*)0)->storageIndex))), 0 },
};

MemberDefinition MembersOf_CollisionVolumeGroup[] = {
	{ MetaType_CollisionVolume, "volumes", u32(reinterpret_cast<uptr>(&(((CollisionVolumeGroup*)0)->volumes))), 1 },
	{ MetaType_u32, "volumeCount", u32(reinterpret_cast<uptr>(&(((CollisionVolumeGroup*)0)->volumeCount))), 0 },
	{ MetaType_CollisionVolume, "totalVolume", u32(reinterpret_cast<uptr>(&(((CollisionVolumeGroup*)0)->totalVolume))), 0 },
};

