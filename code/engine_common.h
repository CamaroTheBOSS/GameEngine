#pragma once
#include <cstdint>
#include <utility>
#include <math.h>

#if !defined(COMPILER_MSVC)
#define COMPILER_MSVC 0
#endif

#if !defined(COMPILER_LLVM)
#define COMPILER_LLVM 0
#endif

#if !defined(COMPILER_GPLUSPLUS)
#define COMPILER_GPLUSPLUS 0
#endif

#if !COMPILER_MSVC && !COMPILER_LLVM && !COMPILER_GPLUSPLUS
#if defined(_MSC_VER) && !defined(__clang__)
#undef COMPILER_MSVC
#define COMPILER_MSVC 1
#endif
#if defined(__GNUC__)
#undef COMPILER_GPLUSPLUS
#define COMPILER_GPLUSPLUS 1
#endif
#if defined(__clang__)
#undef COMPILER_LLVM
#define COMPILER_LLVM 1
#endif
#endif

#define internal static
#define noapi
#define PI 3.14159f
#define kB(bytes) ((bytes) * 1024)
#define MB(bytes) (kB(bytes) * 1024)
#define GB(bytes) (MB(bytes) * 1024)
#define TB(bytes) (GB(bytes) * 1024)
#if COMPILER_MSVC == 1
	#define Assert(expression) if (!(expression)) { *(char*)0 = 0; }
#else
	#define Assert(expression) if (!(expression)) { __builtin_trap(); }
#endif
#define InvalidDefaultCase default: { Assert(0) } break
#define scast(type, expression) static_cast<type>(expression)
#define f4(expression) static_cast<f32>(expression)
#define u4(expression) static_cast<u32>(expression)
#define i4(expression) static_cast<i32>(expression)
#define ptrcast(type, expression) reinterpret_cast<type*>(expression)
#define ArrayCount(arr) (sizeof(arr) / sizeof(arr[0]))
#define Minimum(a, b) ((a) > (b) ? (b) : (a))
#define Maximum(a, b) ((a) > (b) ? (a) : (b))
#define MY_MAX_PATH 255
#define U32_MAX u4(0xFFFFFFFF)
#define I32_MAX i4(U32_MAX >> 1)
#define F32_MAX f4(U32_MAX)


typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef short i16;
typedef int i32;
typedef long long i64;
typedef float f32;
typedef double f64;

#include "engine_intrinsics.h"
#include "engine_math.h"

struct MemoryArena {
	u8* data;
	u64 capacity;
	u64 used;

	u32 tempCount;
};

struct TemporaryMemory {
	MemoryArena* arena;
	u64 usedFingerprint;
};

inline 
void InitializeArena(MemoryArena& arena, void* data, u64 capacity) {
	arena.data = ptrcast(u8, data);
	arena.capacity = capacity;
	arena.used = 0;
	arena.tempCount = 0;
}

inline
TemporaryMemory BeginTempMemory(MemoryArena& arena) {
	TemporaryMemory tempMemory = {};
	tempMemory.arena = &arena;
	tempMemory.usedFingerprint = arena.used;
	arena.tempCount++;
	return tempMemory;
}

inline
void EndTempMemory(TemporaryMemory& memory) {
	Assert(memory.arena->used >= memory.usedFingerprint);
	Assert(memory.arena->tempCount > 0);
	memory.arena->used = memory.usedFingerprint;
	memory.arena->tempCount--;
}

inline
void* PushSize_(MemoryArena & arena, u64 size) {
	Assert(arena.used + size <= arena.capacity);
	void* ptr = arena.data + arena.used;
	arena.used += size;
	return ptr;
}

inline
void ZeroSize_(u8* ptr, u64 size) {
	while (size--) {
		*ptr++ = 0;
	}
}

inline
void CheckArena(MemoryArena& arena) {
	Assert(arena.tempCount == 0);
}

#define ZeroStruct(obj) ZeroSize_(ptrcast(u8, &obj), sizeof(obj))
#define PushStructSize(arena, type) ptrcast(type, PushSize_(arena, sizeof(type)))
#define PushArray(arena, length, type) ptrcast(type, PushSize_(arena, (length) * sizeof(type)))