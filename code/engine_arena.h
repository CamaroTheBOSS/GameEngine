#pragma once
#include "engine_common.h"

#define DEFAULT_ALIGNMENT 4

inline
void InitializeArena_(MemoryArena& arena, void* data, u64 capacity) {
	arena.data = ptrcast(u8, data);
	arena.capacity = capacity;
	arena.used = 0;
	arena.tempCount = 0;
}

inline
void ResetArena(MemoryArena& arena) {
	Assert(arena.tempCount == 0);
	arena.data = arena.data - arena.used;
	arena.used = 0;
	RecordMemoryDebugEvent(Event_MemoryArenaUpdate, arena);
}

inline
u64 GetAlignmentOffset(MemoryArena& arena, u64 alignment) {
	u64 alignMask = alignment - 1;
	Assert((alignment & alignMask) == 0);
	u64 basePtr = reinterpret_cast<uptr>(arena.data) + arena.used;
	u64 alignmentOffset = (alignment - (basePtr & alignMask)) & alignMask;
	return alignmentOffset;
}

inline
bool HasArenaSpaceFor(MemoryArena& arena, u64 size, u64 alignment = DEFAULT_ALIGNMENT) {
	u64 alignmentOffset = GetAlignmentOffset(arena, alignment);
	size += alignmentOffset;
	bool result = (arena.used + size) <= arena.capacity;
	return result;
}

inline
u64 GetArenaFreeSpaceSize(MemoryArena& arena) {
	u64 result = arena.capacity - arena.used;
	return result;
}

inline
TemporaryMemory BeginTempMemory(MemoryArena& arena) {
	TemporaryMemory tempMemory = {};
	tempMemory.arena = &arena;
	tempMemory.usedFingerprint = arena.used;
	arena.tempCount++;
	RecordMemoryDebugEvent(Event_MemoryArenaUpdate, arena);
	return tempMemory;
}

inline
void CommitTempMemory(TemporaryMemory& memory) {
	// TODO: Support for nested temp memory by storing tempCount in temp memory
	Assert(memory.arena->used >= memory.usedFingerprint);
	Assert(memory.arena->tempCount == 1 && "Commiting temp memory does not support nesting temp memory");
	memory.arena->tempCount--;
	RecordMemoryDebugEvent(Event_MemoryArenaUpdate, *memory.arena);
}

inline
void EndTempMemory(TemporaryMemory& memory) {
	Assert(memory.arena->used >= memory.usedFingerprint);
	Assert(memory.arena->tempCount > 0);
	memory.arena->used = memory.usedFingerprint;
	memory.arena->tempCount--;
	RecordMemoryDebugEvent(Event_MemoryArenaUpdate, *memory.arena);
}

inline
void* PushSize_(MemoryArena& arena, u64 size, u64 alignment = DEFAULT_ALIGNMENT) {
	u64 alignmentOffset = GetAlignmentOffset(arena, alignment);
	size += alignmentOffset;
	if (arena.used + size > arena.capacity) {
		Assert(false);
		return 0;
	}
	void* ptr = arena.data + arena.used + alignmentOffset;
	arena.used += size;
	return ptr;
}

inline
void SubArena_(MemoryArena& subArena, MemoryArena& arena, u64 size, u64 alignment = DEFAULT_ALIGNMENT) {
	Assert(subArena.capacity == 0);
	void* data = PushSize_(arena, size, alignment);
	InitializeArena_(subArena, data, size);
}

inline
void ZeroSize_(u8* ptr, u64 size) {
	while (size--) {
		*ptr++ = 0;
	}
}

inline
u8* CopySize(const void* srcv, void* dstv, u32 size) {
	const u8* src = ptrcast(const u8, srcv);
	u8* dst = ptrcast(u8, dstv);
	u8* result = dst;
	while (size--) {
		*dst++ = *src++;
	}
	return result;
}

inline
u8* SafeCopySize(const u8* src, u32 srcSize, u8* dst, u32 dstSize) {
	u32 size = Minimum(srcSize, dstSize);
	u8* result = CopySize(src, dst, size);
	return result;
}

inline
void CheckArena(MemoryArena& arena) {
	Assert(arena.tempCount == 0);
}

#define ZeroStruct(obj) ZeroSize_(ptrcast(u8, &obj), sizeof(obj))
#define PushStructSize(arena, type, ...) ptrcast(type, PushSize_(arena, sizeof(type), ##__VA_ARGS__)); \
	{ RecordMemoryDebugEvent(Event_MemoryArenaUpdate, arena) }
#define PushSize(arena, size, ...) PushSize_(arena, size, ##__VA_ARGS__); \
	{ RecordMemoryDebugEvent(Event_MemoryArenaUpdate, arena) }
#define PushArray(arena, length, type, ...) ptrcast(type, PushSize_(arena, (length) * sizeof(type), ##__VA_ARGS__)); \
	{ RecordMemoryDebugEvent(Event_MemoryArenaUpdate, arena) }
#define PushString(arena, string, size, ...) ptrcast(char, CopySize(ptrcast(const u8,string), PushSize_(arena, (size) * sizeof(char), ##__VA_ARGS__), size)); \
	{ RecordMemoryDebugEvent(Event_MemoryArenaUpdate, arena) }
#define InitializeArena(arena, data, capacity) InitializeArena_(arena, data, capacity); \
	{ RecordMemoryDebugEvent(Event_MemoryArenaInitialize, arena) }
#define SubArena(subarena, arena, capacity) SubArena_(subarena, arena, capacity); \
	{ RecordSubArenaDebugEvent(subarena, arena) }