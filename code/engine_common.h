#pragma once
#include <cstdint>
#include <utility>
#include <math.h>
#include <span>

#pragma once
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
#if defined(_MSC_VER)
#define COMPILER_MSVC 1
#endif
#if defined(__GNUC__)
#define COMPILER_GPLUSPLUS 1
#endif
#if defined(__clang__)
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
#define Assert(expression) if (!(expression)) { *(char*)0 = 0; }
#define scast(type, expression) static_cast<type>(expression)
#define ptrcast(type, expression) reinterpret_cast<type*>(expression)
#define ArrayCount(arr) (sizeof(arr) / sizeof(arr[0]))
#define Minimum(a, b) ((a) > (b) ? (b) : (a))
#define Maximum(a, b) ((a) > (b) ? (a) : (b))
#define MY_MAX_PATH 255

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
};

inline internal
void* PushSize_(MemoryArena & arena, u64 size) {
	Assert(arena.used + size <= arena.capacity);
	void* ptr = arena.data + arena.used;
	arena.used += size;
	return ptr;
}
#define PushStructSize(arena, type) PushSize_(arena, sizeof(type))
#define PushArray(arena, length, type) PushSize_(arena, (length) * sizeof(type))