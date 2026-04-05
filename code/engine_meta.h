#pragma once
#include "engine_common.h"

#define Introspect(parameters)

enum MetaType {
	MetaType_u32,
	MetaType_i32,
	MetaType_f32,
	MetaType_V2,
	MetaType_V3,
	MetaType_V4,

	MetaType_Entity,
	MetaType_HitPoints,
	MetaType_CollisionVolumeGroup,
	MetaType_CollisionVolume,
	MetaType_WorldPosition,
	MetaType_EntityType
};

enum MemberDefinitionFlags {
	MemberDefinitionFlag_IsPointer = 0x1,
	MemberDefinitionFlag_IsReference = 0x2,
};

struct MemberDefinition {
	MetaType type;
	const char* name;
	u32 offset;
	u32 flags;
};

// ------------------- GENERATED STUFF -------------------
// TODO: Make this automatically generated when added fields
extern MemberDefinition MembersOf_Entity[14];
extern MemberDefinition MembersOf_CollisionVolumeGroup[3];