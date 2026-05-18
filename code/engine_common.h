#pragma once
#include <stdint.h>
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
#if !defined(internal)
#define internal static
#define local_persist static
#define debug_variable static
#endif
#define PI 3.14159f
#define TAU 6.28318f
#define kB(bytes) ((bytes) * 1024)
#define MB(bytes) (kB(bytes) * 1024)
#define GB(bytes) (MB(bytes) * 1024)
#define TB(bytes) (GB(bytes) * 1024)
#if defined(INTERNAL_BUILD)
#if COMPILER_MSVC == 1
	#define Assert(expression) if (!(expression)) { *(char*)0 = 0; }
#else
	#define Assert(expression) if (!(expression)) { __builtin_trap(); }
#endif
#else
	#define Assert(expression)
#endif
#define AssertMainThread Assert(debugGlobalMemory->debug.GetCurrThreadId() == 0)
#define InvalidDefaultCase default: { Assert(0) } break
#define scast(type, expression) static_cast<type>(expression)
#define rcst(type, expression) reinterpret_cast<type>(expression)
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

#define DLINKED_LIST_INIT(sentinel) \
	(sentinel)->next = sentinel; \
	(sentinel)->prev = sentinel;

#define DLINKED_LIST_ADD(sentinel, node) \
	(node)->next = (sentinel)->next; \
	(node)->prev = sentinel;	\
	(node)->next->prev = node; \
	(node)->prev->next = node;

#define DLINKED_LIST_REMOVE(node) \
	(node)->next->prev = (node)->prev; \
	(node)->prev->next = (node)->next;


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

#include "engine_meta.h"
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

struct TaskWithMemory {
	MemoryArena arena;
	TemporaryMemory memory;
	volatile u32 done;
};

// TODO: Move it to String common file
struct String8 {
	const char* str;
	u32 length;
};

inline
u32 StringLength(const char* str, char terminator) {
	u32 result = 0;
	while (*str++ != terminator) {
		result++;
	}
	return result;
}

inline
u32 StringLength(const char* str) {
	return StringLength(str, 0);
}

inline
String8 String8FromNullTerminated(const char* str) {
	String8 result;
	result.str = str;
	result.length = StringLength(str);
	return result;
}

inline
bool IsEndLine(char c) {
	bool result = (c == '\n') ||
		(c == '\r');
	return result;
}

inline
bool IsWhiteSpace(char c) {
	bool result = (c == ' ') ||
		(c == '\t') ||
		IsEndLine(c);
	return result;
}

inline
bool StringsAreEqual(const char* A, u32 ALength, const char* B, u32 BLength) {
	if (ALength != BLength) {
		return false;
	}
	for (u32 idx = 0; idx < ALength; idx++) {
		if (*A++ != *B++) {
			return false;
		}
	}
	return true;
}

inline
bool StringsAreEqual(const char* A, u32 ALength, const char* B) {
	for (u32 idx = 0; idx < ALength; idx++) {
		if (*B == 0 || *A++ != *B++) {
			return false;
		}
	}
	return true;
}

inline
u64 CopyString(const char* src, u64 srcSize, char* dst, u64 dstSize) {
	u32 copied = 0;
	while (*src != '\0' && copied < dstSize - 1 && copied < srcSize) {
		*dst++ = *src++;
		copied++;
	}
	*dst = '\0';
	return copied;
}

inline
u64 ConcatenateString(char* first, u64 firstSize, char* second, u64 secondSize, char* dst, u64 dstSize) {
	u64 length = CopyString(first, firstSize, dst, dstSize);
	dst += length;
	length += CopyString(second, secondSize, dst, dstSize - length);
	return length;
}

