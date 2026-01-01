#include "engine_common.h"
#include "engine_render.h"

inline 
void* PushRenderEntry_(RenderGroup& group, u32 size) {
	Assert(group.pushBufferSize + size <= group.maxPushBufferSize);
	if (group.pushBufferSize + size > group.maxPushBufferSize) {
		return 0;
	}
	void* result = ptrcast(void, group.pushBuffer + group.pushBufferSize);
	group.pushBufferSize += size;
	return result;
}

inline
void PushDrawCall(RenderGroup& group, LoadedBitmap* bitmap, V3 center, V3 rectSize, f32 R, f32 G, f32 B, f32 A, V2 offset) {
	DrawCall* call = ptrcast(DrawCall, PushRenderEntry_(group, sizeof(DrawCall)));
	call->bitmap = bitmap;
	call->center = center;
	call->rectSize = rectSize;
	call->R = R;
	call->G = G;
	call->B = B;
	call->A = A;
	call->offset = offset;
}

inline
void PushBitmap(RenderGroup& group, LoadedBitmap* bitmap, V3 center, f32 A, V2 offset) {
	PushDrawCall(group, bitmap, center, {}, 0, 0, 0, A, offset);
}

inline
void PushRect(RenderGroup& group, V3 center, V3 rectSize, f32 R, f32 G, f32 B, f32 A, V2 offset) {
	PushDrawCall(group, 0, center, rectSize, R, G, B, A, offset);
}

inline
RenderGroup AllocateRenderGroup(MemoryArena& arena, u32 size) {
	RenderGroup result = {};
	result.pushBuffer = PushArray(arena, size, u8);
	result.maxPushBufferSize = size;
	result.pushBufferSize = 0;
	return result;
}