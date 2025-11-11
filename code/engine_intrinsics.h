#pragma once
#include "engine_common.h"

inline
i32 RoundF32ToI32(f32 value) {
	return scast(i32, roundf(value));
}

inline
i32 FloorF32ToI32(f32 value) {
	return scast(i32, floorf(value));
}

inline
u32 FloorF32ToU32(f32 value) {
	return scast(u32, floorf(value));
}

struct BitwiseSearchResult {
	bool found;
	u32 index;
};

inline 
BitwiseSearchResult LeastSignificantHighBit(u32 value) {
	BitwiseSearchResult result = {};
#if COMPILER_MSVC
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
