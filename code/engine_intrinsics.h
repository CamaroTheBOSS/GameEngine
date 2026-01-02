#pragma once
#include "engine_common.h"
#include <intrin.h>

inline
i32 RoundF32ToI32(f32 value) {
	return scast(i32, roundf(value));
}

inline
u32 RoundF32ToU32(f32 value) {
	return scast(u32, roundf(value));
}

inline
i32 FloorF32ToI32(f32 value) {
	return scast(i32, floorf(value));
}

inline
u32 FloorF32ToU32(f32 value) {
	return scast(u32, floorf(value));
}

inline
u32 CeilF32ToU32(f32 value) {
	return scast(u32, ceilf(value));
}

inline
f32 Sin(f32 angle) {
	return sinf(angle);
}

inline
f32 Cos(f32 angle) {
	return cosf(angle);
}

inline
i32 Abs(i32 value) {
	if (value < 0) {
		return -value;
	}
	return value;
}

inline
f32 Abs(f32 value) {
	if (value < 0) {
		return -value;
	}
	return value;
}

inline
f32 SquareRoot(f32 value) {
	return sqrtf(value);
}

struct BitwiseSearchResult {
	bool found;
	u32 index;
};

inline 
BitwiseSearchResult LeastSignificantHighBit(u32 value) {
	BitwiseSearchResult result = {};
#if COMPILER_MSVC || COMPILER_LLVM
	_BitScanForward(ptrcast(unsigned long, &result.index), value);
#else
	u32 mask = 1;
	for (u8 index = 0; index < 32; index++) {
		if (value & mask) {
			result.found = true;
			result.index = index;
			break;
		}
		mask *= mask;
	}
	Assert(!"Slow one, add intrinsic!");
#endif
	return result;
}
