#include "main_header.h"
#include "math.h"

internal
i32 RoundF32ToI32(f32 value) {
	return scast(i32, roundf(value));
}

internal
i32 FloorF32ToI32(f32 value) {
	return scast(i32, floorf(value));
}