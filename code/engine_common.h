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
#define TAU 6.28318f
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
#define u2(expression) static_cast<u16>(expression)
#define u4(expression) static_cast<u32>(expression)
#define i2(expression) static_cast<i16>(expression)
#define i4(expression) static_cast<i32>(expression)
#define ptrcast(type, expression) reinterpret_cast<type*>(expression)
#define ArrayCount(arr) (sizeof(arr) / sizeof(arr[0]))
#define Minimum(a, b) ((a) > (b) ? (b) : (a))
#define Maximum(a, b) ((a) > (b) ? (a) : (b))
#define MY_MAX_PATH 255
#define U16_MAX u2(0xFFFF)
#define U32_MAX u4(0xFFFFFFFF)
#define U64_MAX u64(0xFFFFFFFFFFFFFFFF)
#define I16_MAX i2(U16_MAX >> 1)
#define I32_MAX i4(U32_MAX >> 1)
#define I64_MAX i64(U64_MAX >> 1)
#define F32_MAX f4(U32_MAX)
#define AlignUp32(expr) (((expr) + 31) / 32 * 32)
#define AlignUp8(expr) (((expr) + 7) / 8 * 8)
#define AlignDown32(expr) ((expr) / 32 * 32))
#define AlignDown8(expr) ((expr) / 8 * 8)
#if COMPILER_LLVM == 1
	#define LLVM_MCA_BEGIN(name) __asm volatile("# LLVM-MCA-BEGIN " #name:::"memory");
	#define LLVM_MCA_END(name) __asm volatile("# LLVM-MCA-END " #name:::"memory");
#else
	#define LLVM_MCA_BEGIN(name)
	#define LLVM_MCA_END(name)
#endif

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef uintptr_t uptr;
typedef intptr_t iptr;
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
u64 GetAlignmentOffset(MemoryArena& arena, u64 alignment) {
	u64 alignMask = alignment - 1;
	Assert((alignment & alignMask) == 0);
	u64 basePtr = reinterpret_cast<uptr>(arena.data) + arena.used;
	u64 alignmentOffset = (alignment - (basePtr & alignMask)) & alignMask;
	return alignmentOffset;
}

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
void* PushSize_(MemoryArena & arena, u64 size, u64 alignment = 4) {
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
void SubArena(MemoryArena& subArena, MemoryArena& arena, u64 size, u64 alignment = 4) {
	Assert(subArena.capacity == 0);
	void* data = PushSize_(arena, size, alignment);
	InitializeArena(subArena, data, size);
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
#define PushStructSize(arena, type, ...) ptrcast(type, PushSize_(arena, sizeof(type), ##__VA_ARGS__))
#define PushArray(arena, length, type, ...) ptrcast(type, PushSize_(arena, (length) * sizeof(type), ##__VA_ARGS__))